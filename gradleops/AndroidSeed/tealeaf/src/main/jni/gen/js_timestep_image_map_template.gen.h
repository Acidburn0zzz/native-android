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

#ifndef JS_TIMESTEP_IMAGE_MAP_TEMPLATE_H
#define JS_TIMESTEP_IMAGE_MAP_TEMPLATE_H

#include "include/v8.h"
using namespace v8;

#include "js/js.h"
v8::Handle<v8::FunctionTemplate> js_timestep_image_map_get_template(Isolate *isolate);

#endif //JS_TIMESTEP_IMAGE_MAP_TEMPLATE_H
