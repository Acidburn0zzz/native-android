/* @license
 * This file is part of the Game Closure SDK.
 *
 * The Game Closure SDK is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * The Game Closure SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with the Game Closure SDK.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "platform/native_shim.h"
#include "platform/resource_loader.h"
#include "platform/platform.h"

#include "core/tealeaf_canvas.h"
#include "core/tealeaf_shaders.h"
#include "core/texture_manager.h"
#include "core/events.h"
#include "core/config.h"
#include "core/core.h"

#include "js/js.h"

#include <signal.h>
#include <stdlib.h>

#include "JEnv.h"
#include "NativeScriptException.h"
#include <sstream>
#ifdef DEBUG;
#include "JsV8InspectorClient.h"
#endif;
#include "ArgConverter.h"
#include "AssetExtractor.h"

using namespace tns;
using namespace std;

// TODO: We should really be using RegisterClass() here to load the native shim
// methods ahead of time so that during actual invocation it doesn't need to do
// LoadMethod()

static JavaVM* static_vm = NULL;
static native_shim shim;
static int m_initialized = 0;

JNIEnv* get_env() {
    JNIEnv* env;
    static_vm->AttachCurrentThread(&env, NULL);
    return env;
}

native_shim *get_native_shim() {
    if(shim.instance == NULL) {
        LOG("{native} ERROR: Tried to get native shim when there wasn't one");
#if DEBUG
        *((volatile int*)0) = -1;
#else
        exit(1);
#endif
    }
    shim.env = get_env();
    return &shim;
}

native_shim *get_native_thread_shim(JNIEnv **env) {
    if(shim.instance == NULL) {
        LOG("{native} ERROR: Tried to get native shim when there wasn't one (2)");
#if DEBUG
        *((volatile int*)0) = -1;
#else
        exit(1);
#endif
    }
    *env = get_env();
    return &shim;
}

static void set_native_shim(jobject instance) {
    JNIEnv *env = get_env();
    shim.instance = env->NewGlobalRef(instance);
    jclass type = env->GetObjectClass(instance);
    shim.type = (jclass)env->NewGlobalRef(type);
}

static struct sigaction old[NSIG];

void handle_signal(int signal, siginfo_t* info, void* reserved) {
    native_shim* shim = get_native_shim();
    jmethodID method = shim->env->GetMethodID(shim->type, "logNativeError", "()V");
    shim->env->CallVoidMethod(shim->instance, method);
    if((old[signal].sa_flags & SA_SIGINFO) == SA_SIGINFO) {
        old[signal].sa_sigaction(signal, info, reserved);
    } else {
        old[signal].sa_handler(signal);
    }
}

extern "C" JNIEXPORT  void Java_com_tealeaf_NativeShim_init(JNIEnv *env,
                                          jobject thiz,
                                          jobject shim,
                                          jstring code_host,
                                          jstring tcp_host,
                                          jint code_port,
                                          jint tcp_port,
                                          jstring entry_point,
                                          jstring source_dir,
                                          jint width,
                                          jint height,
                                          jboolean remote_loading,
                                          jstring splash,
                                          jstring simulate_id) {
        if (0 != env->GetJavaVM(&static_vm)) {
            LOG("{native} ERROR: Unable to get Java VM");
        }
        set_native_shim(shim);

        char *entry_str = NULL, *tcp_str, *host_str, *source_str, *simulate_id_str, *splash_str;
        GET_STR(env, entry_point, entry_str);
        GET_STR(env, tcp_host, tcp_str);
        GET_STR(env, code_host, host_str);
        GET_STR(env, source_dir, source_str);
        GET_STR(env, simulate_id, simulate_id_str);
        GET_STR(env, splash, splash_str);

        core_init(entry_str, tcp_str, host_str, tcp_port, code_port, source_str, width, height, remote_loading, splash_str, simulate_id_str);

        free(entry_str);
        free(tcp_str);
        free(host_str);
        free(source_str);
        free(simulate_id_str);
        free(splash_str);
#ifndef DEBUG
        struct sigaction action;
        memset(&action, 0, sizeof(struct sigaction));
        action.sa_sigaction = handle_signal;
        action.sa_flags = SA_RESETHAND;
        sigaction(SIGILL, &action, &old[SIGILL]);
        sigaction(SIGABRT, &action, &old[SIGABRT]);
        sigaction(SIGBUS, &action, &old[SIGBUS]);
        sigaction(SIGFPE, &action, &old[SIGFPE]);
        sigaction(SIGSEGV, &action, &old[SIGSEGV]);
        sigaction(SIGSTKFLT, &action, &old[SIGSTKFLT]);
        sigaction(SIGPIPE, &action, &old[SIGPIPE]);
#endif

        LOG("{native} Initialized native JNI bridge");

        m_initialized = 1;
    }
    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_run(JNIEnv*  env, jobject  thiz) {
        core_run();
    }

   extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_destroy(JNIEnv *env, jobject thiz) {
        if (m_initialized) {
            core_destroy();
        }
    }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_reset(JNIEnv *env, jobject thiz) {
        if (m_initialized) {
            core_reset();
        }
    }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_setSingleShader(JNIEnv *env, jobject thiz, jboolean on) {
        use_single_shader = on;
    }

    extern "C" JNIEXPORT jboolean JNICALL Java_com_tealeaf_NativeShim_initIsolate(JNIEnv *env, jobject thiz) {
        jboolean result;

        if (js_init_isolate()) {
            result = JNI_TRUE;
        } else {
            result = JNI_FALSE;
        }

        return result;
    }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_setHalfsizedTextures(JNIEnv *env, jobject thiz, jboolean on) {
        texture_manager_set_use_halfsized_textures(on);
    }

    extern "C" JNIEXPORT jboolean JNICALL Java_com_tealeaf_NativeShim_initJS(JNIEnv* env, jobject thiz, jstring uri, jstring android_hash) {
        char *uri_str = NULL, *android_hash_str = NULL;
        GET_STR(env, uri, uri_str);
        GET_STR(env, android_hash, android_hash_str);

        bool success = core_init_js(uri_str, android_hash_str, thiz);

        free(uri_str);
        free(android_hash_str);

        jboolean result;

        if (success) {
            result = JNI_TRUE;
        } else {
            result = JNI_FALSE;
        }

        return result;
    }

        extern "C" JNIEXPORT jboolean JNICALL Java_com_tealeaf_NativeShim_runNativeJSScript(JNIEnv* env, jobject thiz) {

            bool success = core_run_native_js_script();
            jboolean result;

            if (success) {
                result = JNI_TRUE;
            } else {
                result = JNI_FALSE;
            }

            return result;
        }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_dispatchEvents(JNIEnv* env, jobject thiz, jobjectArray events) {
        jsize len = env->GetArrayLength(events);
        for (int i = 0; i < len; i++) {
            jbyteArray event = (jbyteArray)env->GetObjectArrayElement(events, i);
            char *event_str;
            UTF8_BYTES_TO_STR(env, event, event_str);
            core_dispatch_event(event_str);
            free(event_str);
        }
    }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_dispatchInputEvents(JNIEnv* env, jobject thiz, jintArray ids, jintArray types, jintArray xs, jintArray ys, jint count) {
        jint *id_ints = env->GetIntArrayElements(ids, 0);
        jint *type_ints = env->GetIntArrayElements(types, 0);
        jint *x_ints = env->GetIntArrayElements(xs, 0);
        jint *y_ints = env->GetIntArrayElements(ys, 0);
        for (int i = 0; i < count; i++) {
            core_dispatch_input_event(id_ints[i], type_ints[i], x_ints[i], y_ints[i]);
        }
        env->ReleaseIntArrayElements(ids, id_ints, 0);
        env->ReleaseIntArrayElements(types, type_ints, 0);
        env->ReleaseIntArrayElements(xs, x_ints, 0);
        env->ReleaseIntArrayElements(ys, y_ints, 0);
    }

    void contact_list_build(JNIEnv* env, jobjectArray contacts) {

    }
    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_pushContactList(JNIEnv * env, jobject obj, jobjectArray contacts) {
        contact_list_build(env, contacts);
    }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_saveTextures(JNIEnv *env, jobject thiz) {
        texture_manager_save(texture_manager_get());
    }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_reloadTextures(JNIEnv *env, jobject thiz) {
        texture_manager_reload(texture_manager_get());
    }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_reloadCanvases(JNIEnv *env, jobject thiz) {
        texture_manager_reload_canvases(texture_manager_get());
    }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_clearTextures(JNIEnv *env, jobject thiz) {
        texture_manager_clear_textures(texture_manager_get(), true);
    }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_textureManagerMemoryWarning(JNIEnv *env, jobject thiz) {
        texture_manager_memory_warning();
    }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_textureManagerMemoryCritical(JNIEnv *env, jobject thiz) {
        texture_manager_memory_critical();
    }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_textureManagerResetMemoryCritical(JNIEnv *env, jobject thiz) {
        texture_manager_reset_memory_critical();
    }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_textureManagerSetMaxMemory(JNIEnv *env, jobject thiz, jint bytes) {
        texture_manager_set_max_memory(texture_manager_get(), bytes);
    }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_onTextureLoaded(JNIEnv *env, jobject thiz, jbyteArray url, jint name, jint width, jint height, jint original_width, jint original_height, jint num_channels) {

        char *url_str;
        UTF8_BYTES_TO_STR(env, url, url_str);
        //scale is defaulted to 1 currently. i.e. No scaling.
        int scale = 1;
        texture_manager_on_texture_loaded(texture_manager_get(), url_str,  name, width, height, original_width, original_height, num_channels, scale, false, 0, 0);
        free(url_str);
    }

    extern "C" JNIEXPORT void Java_com_tealeaf_NativeShim_onTextureFailedToLoad(JNIEnv *env, jobject thiz, jstring url) {
        char *url_str = NULL;
        GET_STR(env, url, url_str);
        texture_manager_on_texture_failed_to_load(texture_manager_get(), url_str);
        free(url_str);
    }


    extern "C" JNIEXPORT void JNICALL Java_com_tealeaf_NativeShim_resizeScreen(JNIEnv * env, jobject obj,  jint width, jint height) {
        core_on_screen_resize((int)width, (int)height);
    }

    extern "C" JNIEXPORT void JNICALL Java_com_tealeaf_NativeShim_initGL(JNIEnv *env, jobject obj, jint framebuffer_name) {
        core_init_gl((int)framebuffer_name);
    }

    extern "C" JNIEXPORT void JNICALL Java_com_tealeaf_NativeShim_step(JNIEnv * env, jobject obj, jint dt) {
        core_tick((int)dt);
    }

    // Native thread startup/cleanup

    void native_enter_thread() {
        if(static_vm == NULL) {
            LOG("{native} ERROR: Attempt to use JNI bridge before DalvikVM was set up");
        } else {
            get_env();
        }
    }

    void native_leave_thread() {
        if (static_vm != NULL) {
            static_vm->DetachCurrentThread();
        }
    }
    
//called from NativeScript Java file com_tns_AndroidJsV8Inspector

JNIEXPORT extern "C" void Java_com_tealeaf_NativeShim_initInspector(JNIEnv* env, jobject object) {
#ifdef DEBUG
    JsV8InspectorClient::GetInstance()->init();
#endif 
}

JNIEXPORT extern "C" void Java_com_tealeaf_NativeShim_connect(JNIEnv* env, jobject instance, jobject connection) {
#ifdef DEBUG
    JsV8InspectorClient::GetInstance()->connect(connection);
#endif
}

JNIEXPORT extern "C" void Java_com_tealeaf_NativeShim_scheduleBreak(JNIEnv* env, jobject instance) {
#ifdef DEBUG
    JsV8InspectorClient::GetInstance()->scheduleBreak();
#endif
}

JNIEXPORT extern "C" void Java_com_tealeaf_NativeShim_disconnect(JNIEnv* env, jobject instance) {
#ifdef DEBUG
    JsV8InspectorClient::GetInstance()->disconnect();
#endif
}

JNIEXPORT extern "C" void Java_com_tealeaf_NativeShim_dispatchMessage(JNIEnv* env, jobject instance, jstring jMessage) {
#ifdef DEBUG
    std::string message = ArgConverter::jstringToString(jMessage);
    JsV8InspectorClient::GetInstance()->dispatchMessage(message);
#endif;
}

/* JNIEXPORT extern "C" void  Java_com_tealeaf_NativeShim_extractAssets(JNIEnv* env, jobject obj, jstring apk, jstring inputDir, jstring outputDir, jboolean _forceOverwrite) {
    try {
        AssetExtractor::ExtractAssets(env, obj, apk, inputDir, outputDir, _forceOverwrite);
    } catch (NativeScriptException& e) {
        e.ReThrowToJava();
    } catch (std::exception e) {
        stringstream ss;
        ss << "Error: c++ exception: " << e.what() << endl;
        NativeScriptException nsEx(ss.str());
        nsEx.ReThrowToJava();
    } catch (...) {
        NativeScriptException nsEx(std::string("Error: c++ exception!"));
        nsEx.ReThrowToJava();
    }
}
*/

