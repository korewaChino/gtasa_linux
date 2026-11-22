/* jni_fake.h -- Java method dispatch for the GTA:SA oswrapper layer
 *
 * The JNI environment and object model come from jni-host. This file only
 * declares the method dispatcher and the deferred native callbacks.
 */

#ifndef __JNI_FAKE_H__
#define __JNI_FAKE_H__

#include <stddef.h>
#include <stdint.h>

#include "jni_host.h"

extern jni_host *g_jni_host;
extern void *g_jni_env;

/* set when the game asks to quit */
extern volatile int jni_quit_requested;

/* set once the engine reports the loading splash should hide */
extern volatile int jni_frontend_ready;

void jni_dispatch_init(void);

void jni_dispatch(void *userdata, const char *class_name, const char *method_name,
                  const char *signature, void *this_object,
                  const jni_host_value *args, size_t argc,
                  jni_host_value *result);

/* ------------------------------------------------------------------------ */
/* deferred native callbacks: GameNative entry points the Java platform layer
 * would fire back asynchronously. The engine blocks in early boot states until
 * these complete; we queue them from the JNI dispatch and the main loop invokes
 * the matching impl* entry point. */

typedef enum {
  JNI_CB_PLAYLIST_OPEN_COMPLETE = 1, // implOnPlaylistOpenComplete(success, count)
  JNI_CB_ROCKSTAR_INITIAL_COMPLETE,  // implOnRockstarInitialComplete()
  JNI_CB_ROCKSTAR_GATE_COMPLETE,     // implOnRockstarGateComplete(gate, success)
  JNI_CB_ROCKSTAR_SIGNIN_COMPLETE,   // implOnRockstarSignInComplete()
  JNI_CB_ROCKSTAR_SIGNOUT_COMPLETE,  // implOnRockstarSignOutComplete()
} JniCallbackType;

typedef struct {
  JniCallbackType type;
  int arg0, arg1;
} JniCallback;

/* returns 0 when the queue is empty */
int jni_pop_callback(JniCallback *out);

#endif
