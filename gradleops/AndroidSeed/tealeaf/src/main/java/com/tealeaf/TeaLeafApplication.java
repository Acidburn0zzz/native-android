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
package com.tealeaf;
import android.app.Application;
import android.os.Build;
import android.util.Log;
import com.tealeaf.plugin.PluginManager;

import java.io.File;

public class TeaLeafApplication extends Application {

	public static String PACKAGE_NAME;

	@Override
	public void onCreate() {
		super.onCreate();
		PACKAGE_NAME = getApplicationContext().getPackageName();
		PluginManager.init(this);
		PluginManager.callAll("onCreateApplication", this.getApplicationContext());
	}
}
