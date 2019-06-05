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
 
 #include "CallbackHandlers.h"
#include "MetadataNode.h"
#include "JsArgConverter.h"
#include "JsArgToArrayConverter.h"
#include "ArgConverter.h"
#include "Util.h"
#include "V8GlobalHelpers.h"
#include "V8StringConstants.h"
#include "Constants.h"

#include "Version.h"
#include "WeakRef.h"
#include "NativeScriptAssert.h"
#include "SimpleProfiler.h"
#include "SimpleAllocator.h"
#include "ModuleInternal.h"
#include "NativeScriptException.h"
#include "V8NativeScriptExtension.h"
 #include "Runtime.h"
 
 
#include <assert.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string>
#include "core/util/detect.h"
#include "core/timestep/timestep_animate.h"
#include "platform/native_shim.h"

#include "core/config.h"
#include "core/tealeaf_context.h"
#include "core/tealeaf_canvas.h"
#include "core/url_loader.h"
#include "core/core.h"
#include "core/core_js.h"
#include "core/timer.h"

#include "js/js.h"
#include "js/js_native.h"
#include "js/js_navigator.h"
#include "js/js_socket.h"
#include "js/js_timer.h"
#include "js/js_location.h"
#include "js/js_string_cache.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <log.h>

#include "platform/platform.h"
#include "src/libplatform/default-platform.h"
#include "v8-platform.h"
#include "libplatform/libplatform.h"
#include "include/libplatform/libplatform.h"
#include <chrono>
#include <thread>
#include <ArgConverter.h>

#include "libplatform/libplatform-export.h"
#include "libplatform/v8-tracing.h"
#include "v8-platform.h"  // NOLINT(build/include)
#include "v8config.h"
#include "libplatform/libplatform.h"
#include "include/libplatform/libplatform.h"
#ifdef DEBUG
#include "JsV8InspectorClient.h"
#include "v8_inspector/src/inspector/v8-inspector-platform.h"
#endif
#include "src/libplatform/default-platform.h"
#include "v8-platform.h"

#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "js.h"

#include <jni.h>
#include <v8-inspector.h>


#include "File.h"
#include "ArgConverter.h"
#include "JEnv.h"
#include "NativeScriptException.h"
#include <sstream>

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <sys/stat.h>



using namespace v8;
using namespace tns;
using namespace std;

Persistent<Context> m_context;
Isolate *m_isolate = NULL;
v8::Platform* platform_;

bool s_mainThreadInitialized = false;
bool isDebug = false;
v8::Persistent<v8::Function>* m_gcFunc;
volatile bool m_runGC;

// JS Ready flag: Indicates that the JavaScript engine is running (see core/core_js.h)
bool js_ready = false;
string app_package_name;

CEXPORT bool js_is_ready();

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    try {
        Runtime::Init(vm, reserved);
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

    return JNI_VERSION_1_6;
}

#ifdef DEBUG
static int calc_elapsed(struct timeval *mark) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec - mark->tv_sec) * 1000 + (now.tv_usec - mark->tv_usec) / 1000;
}
#define ELAPSED(var) calc_elapsed(&var)
#endif
#define DECL_BENCH(var) struct timeval var
#define MARK(var) gettimeofday(&var, NULL)

/*timing stuff*/
static DECL_BENCH(et);

CEXPORT void eval_str(const char *str, const char * filename) {
    Locker l(m_isolate);
    HandleScope handle_scope(m_isolate);
    if (m_context.IsEmpty()) {
        LOG("{js} ERROR: Could not evaluate. JavaScript engine is not running yet");
        return;
    }

    Handle<Context> context = getContext();
    Context::Scope context_scope(context);
    Handle<String> source = String::NewFromUtf8(m_isolate, str);
    Local<String> file_name = Local<String>::New(m_isolate, STRING_CACHE_none.Get(m_isolate));
    if(0 == strcmp(str, "undefined")) {

    }
    else {
        ExecuteString(source, filename, true, m_isolate);

    }
}


/*************************************************
 * native js functions
 ************************************************/

Persistent<Function> *tickFunction = NULL;


void timer_start(const v8::FunctionCallbackInfo<v8::Value> &args) {
    Isolate *isolate = getIsolate();
    HandleScope handleScope(isolate);
    Handle<Context> context = getContext();
    LOGFN("setTick");
    Context::Scope context_scope(context);

    if (tickFunction == NULL) {
        tickFunction = new Persistent<Function>();
    }

    if (args[0]->IsFunction()) {
        Local<v8::Function> function = Local<Function>::Cast(args[0]);
        tickFunction->Reset(isolate, function);
    }

    LOGFN("end setTick");
    MARK(et);
}

Handle<Value> getGlobalObject(const v8::FunctionCallbackInfo<v8::Value> &args) {
    LOG("get global obj");
    Handle<Context> context = getContext();
    Handle<Value> globalObject = context->Global();
    LOGFN("end get global obj");
    return globalObject;
}

Local<Context> getContext() {
    return m_context.Get(m_isolate);
}

Isolate *getIsolate() {
    return m_isolate;
}

v8::Platform *getPlatform() {
    return platform_;
}

const char* ToCString(const String::Utf8Value& value) {
    return *value ? *value : "<string conversion failed>";
}

static string getFileName(const string& s) {

   char sep = '/';

#ifdef _WIN32
   sep = '\\';
#endif

   size_t i = s.rfind(sep, s.length());
   if (i != string::npos) {
      return(s.substr(i+1, s.length() - i));
   }

   return("");
}

static string getAppPackageName(){

        if(app_package_name == ""){
        JNIEnv* env = get_env();
        jclass inspectorStarterClass = env->FindClass("com/tealeaf/NativeShim");
        jmethodID getResourcesMethod = env->GetStaticMethodID(inspectorStarterClass, "getPackageName", "()Ljava/lang/String;");
        env->CallStaticObjectMethod(inspectorStarterClass, getResourcesMethod);
        jstring packageName= (jstring)env->CallStaticObjectMethod(inspectorStarterClass, getResourcesMethod);
        const char* pname = env->GetStringUTFChars(packageName, NULL);
        app_package_name = string(pname);
        }
        return app_package_name;
}


std::string GetDirectory (const std::string& path)
{
    size_t found = path.find_last_of("/\\");
    return(path.substr(0, found));
}

void eraseSubStr(std::string & mainStr, const std::string & toErase)
{
	// Search for the substring in string
	size_t pos = mainStr.find(toErase);
 
	if (pos != std::string::npos)
	{
		// If found then erase it from string
		mainStr.erase(pos, toErase.length());
	}
}

// Executes a string within the current v8 context.
Handle<Value> ExecuteString(v8::Handle<v8::String> source, const char * file_name, bool report_exceptions, Isolate *isolate) {
    EscapableHandleScope handle_scope(isolate);
    v8::TryCatch try_catch(isolate);
    
        Persistent<String> sourceP;
        sourceP.Reset(isolate, source);

         std::string filename = "";
        #if defined(DEBUG)
        std::string cStrName(file_name);

        if(cStrName == "native.js"){
        filename = "/data/data/"+getAppPackageName()+"/files/resources/native.js";
        }
        else {
        eraseSubStr( cStrName, "http://");
        std::replace(cStrName.begin(), cStrName.end(), '.', '_');
        std::replace(cStrName.begin(), cStrName.end(), '-', '_');
        std::replace(cStrName.begin(), cStrName.end(), '/', '_');
       // filename = "/data/data/"+getAppPackageName()+"/files/resources/" +cStrName; //+"/"+ getFileName(string(cStrName)
      //  const char *dirName = GetDirectory(filename).c_str();
       // int result_code = mkdir(GetDirectory(dirName).c_str(), 0770);
        
        
        filename = "/data/data/"+getAppPackageName()+"/files/resources/"+ string(cStrName);
        const char *dirName = GetDirectory(filename).c_str();
      
      ////////
        if (File::Exists(filename)) {
                remove(filename.c_str());
        }
        //savefile
        std::ofstream outfile ( filename);
        outfile << ToCString(String::Utf8Value(isolate, sourceP.Get(isolate))) << std::endl;
        outfile.close(); 
        }
        #endif


 v8::MaybeLocal<v8::Script> script;

    if(filename == "" || filename == "entry_point.js"){
        script = v8::Script::Compile(getContext(), sourceP.Get(isolate));
    }
    else{
    #if defined(DEBUG)
        auto src = File::ReadText(filename);
        auto originName = "file://" + filename;
        v8::ScriptOrigin origin(ArgConverter::ConvertToV8String(isolate, originName));
        script = v8::Script::Compile(getContext(), sourceP.Get(isolate), &origin);

        #else
        script = v8::Script::Compile(getContext(), sourceP.Get(isolate));
          #endif // DEBUG
    }


    if (script.IsEmpty()) {
        // Print errors that happened during compilation.
        if (report_exceptions)
            sourceP.Reset();
            ReportException(&try_catch);
        return Undefined(m_isolate);
    } else {
        v8::MaybeLocal<v8::Value> result = script.ToLocalChecked()->Run(getContext());
        if (result.IsEmpty()) {
            assert(try_catch.HasCaught());
            // Print errors that happened during execution.
            if (report_exceptions)
                ReportException(&try_catch);
            return Undefined(m_isolate);
        } else {
            Handle<Context> context = getContext();
            assert(!try_catch.HasCaught());
            return handle_scope.Escape(result.ToLocalChecked());
        }
        sourceP.Reset();
    }
}

static inline void log_error(const char *message) {
    if(!js_is_ready()) {
        return;
    }
    Handle<Context> context = getContext();
    Handle<Object> global = context->Global();
    bool logged = false;
    if (!global.IsEmpty()) {
        Handle<Object> native = (global->Get(STRING_CACHE_NATIVE.Get(m_isolate))->ToObject(context)).ToLocalChecked();
        if (!native.IsEmpty() && native->IsObject()) {
            Handle<Function> log = Handle<Function>::Cast(native->Get(STRING_CACHE___log.Get(m_isolate)));
            if (!log.IsEmpty() && log->IsFunction()) {
                Handle<Value> args[] = { STRING_CACHE_ERROR.Get(m_isolate), String::NewFromUtf8(m_isolate,message) };
                Handle<Value> ret = log->Call(global, 2, args);
                if (!ret.IsEmpty()) {
                    logged = true;
                }
            }
        }
    }
    if (!logged) {
        LOG("%s", message);
    }
}

static inline void window_on_error(const char *msg, const char *url, int line_number) {
    Handle<Context> context = getContext();
    Handle<Object> global = context->Global();
    if (!global.IsEmpty()) {
        Handle<Function> on_error =  Local<Function>::Cast(global->Get(STRING_CACHE_onerror.Get(m_isolate)));
        if (!on_error.IsEmpty() && on_error->IsFunction()) {
            Handle<Value> args[] = { String::NewFromUtf8(m_isolate,msg), String::NewFromUtf8(m_isolate, url), Number::New(m_isolate, line_number) };
            on_error->Call(global, 3, args);
        }
    }
}

static void remote_log_error(const char *message, const char *url, int line_number) {
    native_shim *shim = get_native_shim();
    JNIEnv *env = shim->env;
    const char *sig = "(Ljava/lang/String;Ljava/lang/String;I)V";
    jmethodID method = env->GetMethodID(shim->type, "logJavascriptError", sig);
    jstring jmessage = env->NewStringUTF(message);
    jstring jurl = env->NewStringUTF(url);
    jint jline_number = (jint)line_number;
    env->CallVoidMethod(shim->instance, method, jmessage, jurl, jline_number);
    env->DeleteLocalRef(jmessage);
    env->DeleteLocalRef(jurl);
}


//// JS Object Wrapper

void js_object_wrapper_init(PERSISTENT_JS_OBJECT_WRAPPER *obj) {
    if (!obj->IsEmpty()) {
        obj->Empty();
        obj->Reset();
    }
}

void js_object_wrapper_root(PERSISTENT_JS_OBJECT_WRAPPER *obj, JS_OBJECT_WRAPPER target) {
    js_object_wrapper_delete(obj);
    obj->Reset(m_isolate, target);
}

void js_object_wrapper_delete(PERSISTENT_JS_OBJECT_WRAPPER *obj) {
    if (!obj->IsEmpty()) {
        obj->Reset();
    }
}


#ifndef RELEASE
DECL_BENCH(gc_bench);
static const char *m_gc_type = "Unknown";
#endif

class ClearWeakPersistentHandleVisitor : public PersistentHandleVisitor
{
    public:
        ClearWeakPersistentHandleVisitor () {} //PersistentHandleVisitor
        ~ClearWeakPersistentHandleVisitor() {}
      void VisitPersistentHandle(Persistent<Value>* value,
                                           uint16_t class_id) {
                                               value->ClearWeak();
                                               value->MarkActive();
                                           }
};

ClearWeakPersistentHandleVisitor* visitor;

void gc_start(Isolate* isolate, GCType type, GCCallbackFlags flags) {
#ifndef RELEASE
    MARK(gc_bench);

    static const char* types[] = {
            "Unknown",
            "Scavenge",
            "Mark/Sweep/Compact",
            "All"
    };

    m_gc_type = types[type];
#endif
m_isolate->VisitWeakHandles(visitor);
}

void gc_end(Isolate* isolate, GCType type, GCCallbackFlags flags) {
m_isolate->VisitWeakHandles(visitor);
#ifndef RELEASE
    LOGDEBUG("{jsdebug} GC took %dms (%s)", ELAPSED(gc_bench), m_gc_type);
#endif
}

void ReportException(v8::TryCatch* try_catch) {
    LOG("{js} Reporting exception");

    HandleScope handle_scope(m_isolate);
    v8::String::Utf8Value exception(m_isolate, try_catch->Exception());
    v8::Handle<v8::Message> message = try_catch->Message();
    if (message.IsEmpty()) {
        LOG("{js} WARNING: No exception message");

        // V8 didn't provide any extra information about this error; just
        const char* exception_string = ToCString(exception);
        // print the exception.
        log_error(exception_string);
        window_on_error(exception_string, "", -1);
        remote_log_error(exception_string, "", -1);
    } else {
        v8::String::Utf8Value filename(m_isolate, message->GetScriptResourceName());
        const char* filename_string = ToCString(filename);
        int linenum = message->GetLineNumber(getContext()).ToChecked();
        char buf[512];
        snprintf(buf, sizeof(buf), "%s line: %i", filename_string, linenum);
        log_error(buf);
        v8::String::Utf8Value sourceline(m_isolate, message->GetSourceLine(getContext()).ToLocalChecked());
        const char* sourceline_string = ToCString(sourceline);
        log_error(sourceline_string);

        v8::String::Utf8Value stack_trace(m_isolate, try_catch->StackTrace(getContext()).ToLocalChecked());
        const char* stack_trace_string = "no line";
        if (stack_trace.length() > 0) {
            stack_trace_string = ToCString(stack_trace);
            log_error(stack_trace_string);
        }
        window_on_error(stack_trace_string, filename_string, linenum);
        remote_log_error(stack_trace_string, filename_string, linenum);
    }
}

Handle<Function> get_on_resize() {
    Handle<Object> global = getContext()->Global();
    if(!global.IsEmpty()) {
        Handle<Object> native = Handle<Object>::Cast(global->Get(STRING_CACHE_NATIVE.Get(m_isolate)));
        if(!native.IsEmpty()) {
            Handle<Object> screen = Handle<Object>::Cast(native->Get(STRING_CACHE_screen.Get(m_isolate)));
            if(!screen.IsEmpty()) {
                Handle<Function> on_resize = Handle<Function>::Cast(screen->Get(STRING_CACHE_onResize.Get(m_isolate)));
                return on_resize;
            }
        }
    }
    return Handle<Function>();
}

CEXPORT void js_tick(long dt) {
    Locker l(m_isolate);

    if (!js_ready) {
        LOG("{js} WARNING: Tick attempted before JavaScript engine was running");
        return;
    }
    TryCatch try_catch(m_isolate);
    HandleScope handle_scope(m_isolate);
    Handle<Context> context = getContext();
    Context::Scope context_scope(context);


    if (dt > 1000) {
        dt = 1;
    }

    Handle<Object> global = context->Global();
    tealeaf_canvas *canvas = tealeaf_canvas_get();
    if (canvas->should_resize) {
        canvas->should_resize = false;
        Handle<Function> resize = get_on_resize();
        if (!resize.IsEmpty() && resize->IsFunction()) {
            int screen_width = config_get_screen_width();
            int screen_height = config_get_screen_height();
            Handle<Value> args[] = { Integer::New(m_isolate, screen_width), Integer::New(m_isolate, screen_height) };
            Handle<Value> ret = resize->Call(global, 2, args);
            if (ret.IsEmpty()) {
                ReportException(&try_catch);
            }
        }
    }

    if (tickFunction == NULL) {
        return;
    }

    view_animation_tick_animations(dt, m_isolate);
    Handle<Function> func = tickFunction->Get(m_isolate);
    if (func->IsFunction()) {
        Handle<Value> args[] = { Number::New(m_isolate, dt) };
        Handle<Value> ret = func->Call(global, 1, args);
        if (ret.IsEmpty()) {
            ReportException(&try_catch);
        }
    } else {
        LOG("{js} ERROR: Invalid tick callback");
    }
}

CEXPORT bool js_is_ready() {
    return js_ready;
}

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
public:
    virtual void* Allocate(size_t length) {
        void* data = AllocateUninitialized(length);
        return data == NULL ? data : memset(data, 0, length);
    }
    virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
    virtual void Free(void* data, size_t) { free(data); }
};




// Call this from the main thread of javascript execution, so that the isolate
// is tied to the correct thread id.  This is important for profiling because
// the V8 profiler internally will send SIGPROF to this thread.
bool js_init_isolate() {
    DECL_BENCH(t);
    MARK(t);
 #if defined(DEBUG)
       isDebug = true;
    #else // DEBUG
        LOG("{debugger} JavaScript Debug Server is disabled");
    #endif // DEBUG


    // Initialize V8.
    v8::V8::InitializeICUDefaultLocation(nullptr);
    v8::V8::InitializeExternalStartupData(nullptr);
    #if defined(DEBUG)
     // The default V8 platform isn't Chrome DevTools compatible. The frontend uses the
        // Runtime.evaluate protocol command with timeout flag for every execution in the console.
        // The default platform doesn't implement executing delayed javascript code from a background
        // thread. To avoid implementing a full blown scheduler, we use the default platform with a
        // timeout=0 flag.
        platform_ =  V8InspectorPlatform::CreateDefaultPlatform();
     #else // DEBUG
        platform_ = v8::platform::CreateDefaultPlatform();
    #endif // DEBUG
    Runtime::platform = platform_;
    v8::V8::InitializePlatform(platform_);
    v8::V8::Initialize();
    LOG("v8engine inited");

    // Create a new Isolate and make it the current one.
    ArrayBufferAllocator allocator;
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = &allocator;
    m_isolate = v8::Isolate::New(create_params);
    if (m_isolate) {
        LOG("{js} Initial setup took %ims", ELAPSED(t));
        return true;
    } else {
        LOG("{js} ERROR: Unable to get current isolate");
        return false;
    }
}



bool init_js(const char *uri, const char *native_hash, jobject thiz) {
    DECL_BENCH(t);
     Isolate::Scope isolate_scope(m_isolate);
    HandleScope handleScope(m_isolate);

    // Sets a structure with v8 String constants on the isolate object at slot 1
    V8StringConstants::PerIsolateV8Constants* consts = new V8StringConstants::PerIsolateV8Constants(m_isolate);
    m_isolate->SetData((uint32_t)Runtime::IsolateData::CONSTANTS, consts);

    V8::SetFlagsFromString(Constants::V8_STARTUP_FLAGS.c_str(), Constants::V8_STARTUP_FLAGS.size());
    m_isolate->SetCaptureStackTraceForUncaughtExceptions(true, 100, StackTrace::kOverview);
    m_isolate->AddMessageListener(NativeScriptException::OnUncaughtError);

    __android_log_print(ANDROID_LOG_DEBUG, "TNS.Native", "V8 version %s", V8::GetVersion());

    m_isolate->AddGCPrologueCallback(gc_start, GCType::kGCTypeAll);
    m_isolate->AddGCEpilogueCallback(gc_end, GCType::kGCTypeAll);
    MARK(t);
    Local<ObjectTemplate> global = ObjectTemplate::New(m_isolate);

    js_string_cache_init(m_isolate);

    // set the global object's functions *before* Context::New creates an instance, otherwise we just don't get them
    global->Set(STRING_CACHE_setTimeout.Get(m_isolate), FunctionTemplate::New(m_isolate, defSetTimeout));
    global->Set(STRING_CACHE_clearTimeout.Get(m_isolate), FunctionTemplate::New(m_isolate, defClearTimeout));
    global->Set(STRING_CACHE_setInterval.Get(m_isolate), FunctionTemplate::New(m_isolate, defSetInterval));
    global->Set(STRING_CACHE_clearInterval.Get(m_isolate), FunctionTemplate::New(m_isolate, defClearInterval));
    global->Set(STRING_CACHE_setLocation.Get(m_isolate), FunctionTemplate::New(m_isolate, native_set_location));

#if defined DEBUG

const auto readOnlyFlags = static_cast<PropertyAttribute>(PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);



    global->Set(ArgConverter::ConvertToV8String(m_isolate, "__log"), FunctionTemplate::New(m_isolate, CallbackHandlers::LogMethodCallback));
    global->Set(ArgConverter::ConvertToV8String(m_isolate, "__dumpReferenceTables"), FunctionTemplate::New(m_isolate, CallbackHandlers::DumpReferenceTablesMethodCallback));
    global->Set(ArgConverter::ConvertToV8String(m_isolate, "__enableVerboseLogging"), FunctionTemplate::New(m_isolate, CallbackHandlers::EnableVerboseLoggingMethodCallback));
    global->Set(ArgConverter::ConvertToV8String(m_isolate, "__disableVerboseLogging"), FunctionTemplate::New(m_isolate, CallbackHandlers::DisableVerboseLoggingMethodCallback));
    global->Set(ArgConverter::ConvertToV8String(m_isolate, "__exit"), FunctionTemplate::New(m_isolate, CallbackHandlers::ExitMethodCallback));
    global->Set(ArgConverter::ConvertToV8String(m_isolate, "__runtimeVersion"), ArgConverter::ConvertToV8String(m_isolate, NATIVE_SCRIPT_RUNTIME_VERSION), readOnlyFlags);
    global->Set(ArgConverter::ConvertToV8String(m_isolate, "__time"), FunctionTemplate::New(m_isolate, CallbackHandlers::TimeCallback));

/*
     * Attach `Worker` object constructor only to the main thread (m_isolate)'s global object
     * Workers should not be created from within other Workers, for now
     */

    if (!s_mainThreadInitialized) {
        Local<FunctionTemplate> workerFuncTemplate = FunctionTemplate::New(m_isolate, CallbackHandlers::NewThreadCallback);
        Local<ObjectTemplate> prototype = workerFuncTemplate->PrototypeTemplate();

/*
         * Attach methods from the EventTarget interface (postMessage, terminate) to the Worker object prototype
         */

        auto postMessageFuncTemplate = FunctionTemplate::New(m_isolate, CallbackHandlers::WorkerObjectPostMessageCallback);
        auto terminateWorkerFuncTemplate = FunctionTemplate::New(m_isolate, CallbackHandlers::WorkerObjectTerminateCallback);

        prototype->Set(ArgConverter::ConvertToV8String(m_isolate, "postMessage"), postMessageFuncTemplate);
        prototype->Set(ArgConverter::ConvertToV8String(m_isolate, "terminate"), terminateWorkerFuncTemplate);

        global->Set(ArgConverter::ConvertToV8String(m_isolate, "Worker"), workerFuncTemplate);
    }
/*
     * Emulate a `WorkerGlobalScope`
     * Attach 'postMessage', 'close' to the global object
     */

    else {
        auto postMessageFuncTemplate = FunctionTemplate::New(m_isolate, CallbackHandlers::WorkerGlobalPostMessageCallback);
        global->Set(ArgConverter::ConvertToV8String(m_isolate, "postMessage"), postMessageFuncTemplate);
        auto closeFuncTemplate = FunctionTemplate::New(m_isolate, CallbackHandlers::WorkerGlobalCloseCallback);
        global->Set(ArgConverter::ConvertToV8String(m_isolate, "close"), closeFuncTemplate);
    }
    
   SimpleProfiler::Init(m_isolate, global);
   CallbackHandlers::CreateGlobalCastFunctions(m_isolate, global);

#endif

    m_context.Reset(m_isolate, Context::New(m_isolate, NULL, global));
    m_context.Get(m_isolate)->Enter();


    #if defined(DEBUG)
       JsV8InspectorClient::GetInstance()->init();
       JsV8InspectorClient::attachInspectorCallbacks(m_isolate, global);
    #else // DEBUG
        LOG("{debugger} JavaScript Debug Server is disabled");
    #endif // DEBUG


auto gcFunc = getContext()->Global()->Get(ArgConverter::ConvertToV8String(m_isolate, "gc"));
    if (!gcFunc.IsEmpty() && gcFunc->IsFunction()) {
        m_gcFunc = new Persistent<Function>(m_isolate, gcFunc.As<Function>());
        Local<Value> recv;
        gcFunc.As<Function>()->Call( recv, 0, 0);
    }
  
    if (m_context.IsEmpty()) {
        LOG("{js} ERROR: Unable to create context");
        return false;
    }




#if defined(REMOTE_DEBUG)
    // Todo: put inspector initialization here


#else // REMOTE_DEBUG
    LOG("{debugger} JavaScript Debug Server is disabled");
#endif // REMOTE_DEBUG

    Local<Context> context = getContext();
    Context::Scope scope(context);

    v8::HandleScope handle_scope(getIsolate());


    // config
    Handle<ObjectTemplate> config = v8::ObjectTemplate::New(getIsolate());
    config->Set(m_isolate, "baseURL", String::NewFromUtf8(getIsolate(), config_get_code_host()));

    // window.location
    native_initialize_location(config_get_code_host(), m_isolate);

    // screen
    int width = config_get_screen_width();
    int height = config_get_screen_height();

    Handle<ObjectTemplate> screen = ObjectTemplate::New(getIsolate());
    screen->Set(STRING_CACHE_width.Get(getIsolate()), Number::New(getIsolate(),width));
    screen->Set(STRING_CACHE_height.Get(getIsolate()), Number::New(getIsolate(),height));

    // timer
    Handle<ObjectTemplate> timer = ObjectTemplate::New(getIsolate());
    timer->Set(STRING_CACHE_start.Get(getIsolate()), FunctionTemplate::New(getIsolate(), timer_start));

    // navigator
    Handle<ObjectTemplate> navigator = js_navigator_get_template(m_isolate);

    // native
    Handle<ObjectTemplate> NATIVE = js_native_get_template(uri, native_hash, m_isolate);
    NATIVE->Set(STRING_CACHE_timer.Get(m_isolate), timer);

    // global_object
    Local<Object> global_object = m_context.Get(m_isolate)->Global();

    global_object->Set(STRING_CACHE_CONFIG.Get(m_isolate), config->NewInstance());
    global_object->Set(STRING_CACHE_screen.Get(m_isolate), screen->NewInstance());
    global_object->Set(STRING_CACHE_navigator.Get(m_isolate), navigator->NewInstance());
    global_object->Set(STRING_CACHE_window.Get(m_isolate), global_object);
    global_object->Set(STRING_CACHE_NATIVE.Get(m_isolate), NATIVE->NewInstance());
    global_object->Set(STRING_CACHE_GLOBAL.Get(m_isolate), global_object);

    global_object->SetAccessor(context, STRING_CACHE_location.Get(m_isolate), jsGetLocation, jsSetLocation);


#if defined(ENABLE_PROFILER)
    Handle<ObjectTemplate> PROFILER = ObjectTemplate::New();
    nodex::InitializeProfiler(PROFILER);
    global_object->Set(STRING_CACHE_PROFILER, PROFILER->NewInstance());
#endif // ENABLE_PROFILER

    LOG("{js} Initialized in %dms", ELAPSED(t));

    js_ready = true;

s_mainThreadInitialized = true;


    return true;
}

CEXPORT bool destroy_js() {
    LOG("{js} Shutting down");

    if(js_ready) {
        js_ready = false;
        Locker l(m_isolate);

        HandleScope scope(m_isolate);

        // Cleanup timer
        core_timer_clear_all();
        if(tickFunction != NULL && !(*tickFunction).IsEmpty()) {
            (*tickFunction).Reset();
        }
        tickFunction = NULL;


        m_context.Get(getIsolate())->DetachGlobal();
        m_context.Reset();
    }

    return true;
}
