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
#include "js/js_plugins.h"
#include "platform/plugins.h"
#include <stdlib.h> // pulls in declaration of malloc, free
#include <string.h> // pulls in declaration for strlen.

using namespace v8;


void js_plugins_send_event(const v8::FunctionCallbackInfo<v8::Value> &args) {
    Isolate *isolate = getIsolate();
    LOGFN("plugins send event");
    char *ret_str = NULL;
    if (args[0]->IsString() && args[1]->IsString() && args[2]->IsString()) {
        String::Utf8Value str_plugin_class(isolate, args[0]->ToString(isolate));
        String::Utf8Value str_plugin_method(isolate, args[1]->ToString(isolate));
        String::Utf8Value str_data(isolate, args[2]->ToString(isolate));

        const char* plugin_class = ToCString(str_plugin_class);
        const char* plugin_method = ToCString(str_plugin_method);
        const char* data = ToCString(str_data);
        ret_str = plugins_send_event(plugin_class, plugin_method, data);
    } else {
        LOG("{plugins} WARNING: send event should be called with 3 string arguments");
    }

    LOGFN("end plugins send event");
    Handle<Value> retVal;
    if (ret_str != NULL) {
        retVal = String::NewFromUtf8(isolate, ret_str);
        free(ret_str);
    } else {
        retVal = String::NewFromUtf8(isolate, "{}");
    }
    args.GetReturnValue().Set(retVal);
}

void js_plugins_send_request(const v8::FunctionCallbackInfo<v8::Value> &args) {
    Isolate *isolate = getIsolate();
    LOGFN("plugins send request");
    if (args[0]->IsString() && args[1]->IsString() && args[2]->IsString()) {
        String::Utf8Value str_plugin_class(isolate, args[0]->ToString(isolate));
        String::Utf8Value str_plugin_method(isolate, args[1]->ToString(isolate));
        String::Utf8Value str_data(isolate, args[2]->ToString(isolate));

        const char* plugin_class = ToCString(str_plugin_class);
        const char* plugin_method = ToCString(str_plugin_method);
        const char* data = ToCString(str_data);
        int request_id = args[3]->Int32Value(isolate->GetCurrentContext()).ToChecked();
        plugins_send_request(plugin_class, plugin_method, data, request_id);
    } else {
        LOG("{plugins} WARNING: send request should be called with 3 string arguments");
    }
}


Handle<ObjectTemplate> js_plugins_get_template(Isolate *isolate) {
    Handle<ObjectTemplate> actions = ObjectTemplate::New(isolate);
    actions->Set(STRING_CACHE_sendEvent.Get(isolate), FunctionTemplate::New(isolate, js_plugins_send_event));
    actions->Set(STRING_CACHE_sendRequest.Get(isolate), FunctionTemplate::New(isolate, js_plugins_send_request));
    return actions;
}

