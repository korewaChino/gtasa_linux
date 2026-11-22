/* jni_dispatch.c -- oswrapper Java method dispatch for GTA:SA 2.11.311
 *
 * jni-host owns the JNIEnv/JavaVM tables, object model, strings, arrays and
 * fields. This file answers the Java methods the engine calls, keyed by name,
 * and keeps the deferred native callbacks.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "util.h"
#include "jni_fake.h"

jni_host *g_jni_host = NULL;
void *g_jni_env = NULL;

volatile int jni_quit_requested = 0;
volatile int jni_frontend_ready = 0;

// ---------------------------------------------------------------------------
// deferred native-callback queue (the Java->native completion direction)
// ---------------------------------------------------------------------------

#define CB_QUEUE_LEN 32
static JniCallback cb_queue[CB_QUEUE_LEN];
static int cb_head = 0, cb_tail = 0;

static void push_cb(JniCallbackType type, int arg0, int arg1) {
  const int next = (cb_tail + 1) % CB_QUEUE_LEN;
  if (next == cb_head) {
    debugPrintf("JNI: callback queue full, dropping %d\n", type);
    return;
  }
  cb_queue[cb_tail].type = type;
  cb_queue[cb_tail].arg0 = arg0;
  cb_queue[cb_tail].arg1 = arg1;
  cb_tail = next;
}

int jni_pop_callback(JniCallback *out) {
  if (cb_head == cb_tail)
    return 0;
  if (out)
    *out = cb_queue[cb_head];
  cb_head = (cb_head + 1) % CB_QUEUE_LEN;
  return 1;
}

// ---------------------------------------------------------------------------
// app-local key/value store (get/setAppLocalValue), persisted as tab-separated
// lines. Backs STORAGE_ROOT and the engine's misc settings.
// ---------------------------------------------------------------------------

#define KV_MAX 256

typedef struct {
  char key[64];
  char val[256];
} KvPair;

static KvPair kv_store[KV_MAX];
static int kv_count = 0;

static const char *kv_get(const char *key) {
  for (int i = 0; i < kv_count; i++)
    if (!strcmp(kv_store[i].key, key))
      return kv_store[i].val;
  return NULL;
}

static void kv_save(void) {
  FILE *f = fopen(APPSTATE_NAME, "w");
  if (!f)
    return;
  for (int i = 0; i < kv_count; i++)
    fprintf(f, "%s\t%s\n", kv_store[i].key, kv_store[i].val);
  fclose(f);
}

static void kv_set(const char *key, const char *val) {
  for (int i = 0; i < kv_count; i++) {
    if (!strcmp(kv_store[i].key, key)) {
      strlcpy(kv_store[i].val, val, sizeof(kv_store[i].val));
      kv_save();
      return;
    }
  }
  if (kv_count >= KV_MAX) {
    debugPrintf("JNI: app-local store full, dropping %s\n", key);
    return;
  }
  strlcpy(kv_store[kv_count].key, key, sizeof(kv_store[kv_count].key));
  strlcpy(kv_store[kv_count].val, val, sizeof(kv_store[kv_count].val));
  kv_count++;
  kv_save();
}

static void kv_load(void) {
  FILE *f = fopen(APPSTATE_NAME, "r");
  if (!f)
    return;
  char line[360];
  while (kv_count < KV_MAX && fgets(line, sizeof(line), f)) {
    char *tab = strchr(line, '\t');
    if (!tab)
      continue;
    *tab = 0;
    char *val = tab + 1;
    val[strcspn(val, "\r\n")] = 0;
    strlcpy(kv_store[kv_count].key, line, sizeof(kv_store[kv_count].key));
    strlcpy(kv_store[kv_count].val, val, sizeof(kv_store[kv_count].val));
    kv_count++;
  }
  fclose(f);
}

// ---------------------------------------------------------------------------
// dispatch
// ---------------------------------------------------------------------------

void jni_dispatch_init(void) { kv_load(); }

static const char *arg_string(const jni_host_value *args, size_t argc,
                              size_t index) {
  if (!args || index >= argc || !args[index].l)
    return "";
  const char *text = jni_host_string_utf(g_jni_host, args[index].l);
  return text ? text : "";
}

static void set_string(jni_host_value *result, const char *text) {
  result->l = jni_host_new_string(g_jni_host, text);
}

static int returns_object(const char *signature) {
  const char *ret = signature ? strrchr(signature, ')') : NULL;
  return ret && (ret[1] == 'L' || ret[1] == '[');
}

void jni_dispatch(void *userdata, const char *class_name,
                  const char *method_name, const char *signature,
                  void *this_object, const jni_host_value *args, size_t argc,
                  jni_host_value *result) {
  (void)userdata;
  (void)this_object;
  if (!result)
    return;
  memset(result, 0, sizeof(*result));
  if (!method_name)
    return;

  const char *cls = class_name ? class_name : "";

  // app-local key/value store (STORAGE_ROOT etc. are written here at boot)
  if (!strcmp(method_name, "setAppLocalValue")) {
    const char *key = arg_string(args, argc, 0);
    const char *val = arg_string(args, argc, 1);
    kv_set(key, val);
    debugPrintf("JNI: setAppLocalValue(%s = %s)\n", key, val);
    return;
  }

  if (!strcmp(method_name, "getAppLocalValue")) {
    const char *key = arg_string(args, argc, 0);
    if (!strcmp(key, "STORAGE_ROOT") || !strcmp(key, "STORAGE_ROOT_BASE")) {
      set_string(result, ".");
      return;
    }
    const char *value = kv_get(key);
    debugPrintf("JNI: getAppLocalValue(%s) -> %s\n", key,
                value ? value : "(empty)");
    set_string(result, value ? value : "");
    return;
  }

  // splash / loading screen: no Java UI, so these are no-ops. hideSplashScreen
  // is the engine's "loading finished" signal.
  if (!strcmp(method_name, "showSplashScreen") ||
      !strcmp(method_name, "setSplashImage") ||
      !strcmp(method_name, "setSplashText"))
    return;
  if (!strcmp(method_name, "hideSplashScreen")) {
    cpu_boost(0);
    jni_frontend_ready = 1;
    debugPrintf("JNI: hideSplashScreen -> frontend ready\n");
    return;
  }

  // --- async platform operations the engine blocks on until we call back ---
  if (!strcmp(method_name, "playlistOpen")) {
    debugPrintf("JNI: playlistOpen -> queue OnPlaylistOpenComplete(1, 0)\n");
    push_cb(JNI_CB_PLAYLIST_OPEN_COMPLETE, 1, 0);
    return;
  }
  if (!strcmp(method_name, "rockstarShowInitial")) {
    debugPrintf("JNI: rockstarShowInitial -> queue OnRockstarInitialComplete\n");
    push_cb(JNI_CB_ROCKSTAR_INITIAL_COMPLETE, 0, 0);
    return;
  }
  if (!strcmp(method_name, "rockstarShowGate")) {
    const int gate = args && argc > 0 ? args[0].i : 0;
    debugPrintf("JNI: rockstarShowGate(%d) -> queue OnRockstarGateComplete(%d, 1)\n",
                gate, gate);
    push_cb(JNI_CB_ROCKSTAR_GATE_COMPLETE, gate, 1);
    return;
  }
  if (!strcmp(method_name, "rockstarSignIn")) {
    debugPrintf("JNI: rockstarSignIn -> queue OnRockstarSignInComplete\n");
    push_cb(JNI_CB_ROCKSTAR_SIGNIN_COMPLETE, 0, 0);
    return;
  }
  if (!strcmp(method_name, "rockstarSignOut")) {
    debugPrintf("JNI: rockstarSignOut -> queue OnRockstarSignOutComplete\n");
    push_cb(JNI_CB_ROCKSTAR_SIGNOUT_COMPLETE, 0, 0);
    return;
  }

  // lifecycle / misc
  if (!strcmp(method_name, "finish") || !strcmp(method_name, "exitGame") ||
      !strcmp(method_name, "quit") || !strcmp(method_name, "QuitApp")) {
    debugPrintf("JNI: %s -> request quit\n", method_name);
    jni_quit_requested = 1;
    return;
  }

  if (!strcmp(method_name, "getAppVersion") ||
      !strcmp(method_name, "GetVersionName")) {
    set_string(result, "2.11");
    return;
  }
  if (!strcmp(method_name, "GetDeviceLanguage")) {
    set_string(result, "en");
    return;
  }

  // toImage / getInstance / getParameter / other getters -> a fresh fake obj
  if (returns_object(signature)) {
    debugPrintf("JNI: CallObjectMethod %s.%s -> fake object\n", cls,
                method_name);
    result->l = jni_host_new_instance(g_jni_host, "osobject");
    return;
  }

  // Boolean, int and float returns keep the zero written above.
  debugPrintf("JNI: CallMethod %s.%s ignored\n", cls, method_name);
}
