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
#include "js_locale.h"
#include "platform/get_locale.h"
#include "include/v8.h"
using namespace v8;

void js_locale_get_country(Local< String > property, const PropertyCallbackInfo< Value > &info) {
    Isolate *isolate = getIsolate();
    LOGFN("in get locale");
    locale_info *locale = locale_get_locale();
    LOGFN("end native set location");
    info.GetReturnValue().Set(String::NewFromUtf8(isolate, locale->country));
}

void js_locale_get_language(Local< String > property, const PropertyCallbackInfo< Value > &info) {
    Isolate *isolate = getIsolate();
    LOGFN("in get locale");
    locale_info *locale = locale_get_locale();
    LOGFN("end native set location");
    info.GetReturnValue().Set(String::NewFromUtf8(isolate, locale->language));
}


Local<ObjectTemplate> js_locale_get_template(Isolate *isolate) {
    Handle<ObjectTemplate> locale = ObjectTemplate::New(isolate);
    locale->SetAccessor(STRING_CACHE_language.Get(isolate), js_locale_get_language);
    locale->SetAccessor(STRING_CACHE_country.Get(isolate), js_locale_get_country);

    return locale;
}
