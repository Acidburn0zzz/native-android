/* @license
 * This file is part of the Game Closure SDK.
 *
 * The Game Closure SDK is free software: you can redistribute it and/or modify
 * it under the terms of the Mozilla Public License v. 2.0 as published by Mozilla.
 
 * The Game Closure SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Mozilla Public License v. 2.0 for more details.
 
 * You should have received a copy of the Mozilla Public License v. 2.0
 * along with the Game Closure SDK.  If not, see <http://mozilla.org/MPL/2.0/>.
 */
#ifndef JS_H
#define JS_H
#include "include/v8.h"
using namespace v8;
#include "core/core_js.h"

#include "platform/platform.h"
#include "libplatform/libplatform.h"
#include "js/js_string_cache.h"

#define JS_OBJECT_WRAPPER v8::Local<v8::Object>
#define PERSISTENT_JS_OBJECT_WRAPPER v8::Persistent<v8::Object>
extern v8::Platform* platform_;
extern v8::Persistent<v8::Context> m_context;
void js_object_wrapper_init(PERSISTENT_JS_OBJECT_WRAPPER *obj);
void js_object_wrapper_root(PERSISTENT_JS_OBJECT_WRAPPER *obj, JS_OBJECT_WRAPPER target);
void js_object_wrapper_delete(PERSISTENT_JS_OBJECT_WRAPPER *obj);

v8::Handle<v8::Value> log(const v8::internal::Arguments& args);

const char* ToCString(const v8::String::Utf8Value& value);
void ReportException(v8::TryCatch* try_catch);
v8::Handle<v8::Value> ExecuteString(v8::Handle<v8::String> source,
                   const char * filename,
                   bool report_exceptions, Isolate *isolate);

void dispatchData(JNIEnv *env, jint id, jstring data);
void dispatchEvent(jstring event);

bool js_init_isolate();
void setAssetsExtracted();
void printToJavaUTF16(const char* exception_string);

v8::Local<v8::Context> getContext();
v8::Isolate *getIsolate();
v8::Platform *getPlatform();
#endif
