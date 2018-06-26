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

var util = require('util');
var path = require('path');
var spawn = require('child_process').spawn;
var Promise = require('bluebird');
var fs = Promise.promisifyAll(require('fs-extra'));
var chalk = require('chalk');

// unexpected exceptions show stack, build errors should just show the error
// message to the user
var BuildError = function (message, showStack) {
    this.message = chalk.red(message);
    this.showStack = showStack || false;
};

util.inherits(BuildError, Error);

exports.BuildError = BuildError;

var existsAsync = function (filename) {
    return new Promise(function (resolve) {
        if (!filename) { return resolve(false); }

        fs.exists(filename, function (exists) {
            resolve(exists);
        });
    });
};

// used to remove punctuation (if any) from the appid
var PUNCTUATION_REGEX = /[!"#$%&'()*+,\-.\/:;<=>?@\[\\\]^_`{|}~]/g;

var ANDROID_TARGET = "android-27";
var androidVersion = require('./package.json').version;

var logger;

function spawnWithLogger(api, name, args, opts) {
    return new Promise(function (resolve, reject) {
        var logger = api.logging.get(name);
        logger.log(chalk.green(name + ' ' + args.join(' ')));
        var streams = logger.createStreams(['stdout'], false);
        var child = spawn(name, args, opts);
        child.stdout.pipe(streams.stdout);
        child.stderr.pipe(streams.stdout);
        child.on('close', function (code) {
            if (code) {
                var err = new BuildError(chalk.green(name) + chalk.red(' exited with non-zero exit code (' + code + ')'));
                err.stdout = streams.get('stdout');
                err.code = code;
                reject(err);
            } else if (opts && opts.capture) {
                resolve(streams.get('stdout'));
            } else {
                resolve();
            }
        });
    });
}

function legacySpawnWithLogger(api, name, args, opts) {
    var logger = api.logging.get(name);
    logger.log(name, args.join(' '));
    var child = spawn(name, args, opts);
    child.stdout.pipe(logger, {end: false});
    child.stderr.pipe(logger, {end: false});

    var stdout = '';
    if (opts && opts.capture) {
        child.stdout.on('data', function (chunk) {
            stdout += chunk;
        });
    }

    return new Promise(function (resolve, reject) {
        child.on('close', function (err) {
            if (err) {
                reject(err);
            } else {
                resolve(stdout);
            }
        });
    });
}

//// Modules

var getModuleConfig = function(api, app) {
    var moduleConfig = {};
    return Promise.map(Object.keys(app.modules), function (moduleName) {
        var modulePath = app.modules[moduleName].path;
        var configFile = path.join(modulePath, 'android', 'config.json');
        return fs.readFileAsync(configFile, 'utf8')
            .then(function (data) {
                moduleConfig[moduleName] = {
                    config: JSON.parse(data),
                    path: modulePath
                };
            }, function (err) {
                // modules are not required to have a config.json, ignore missing file
                if (err && err.code !== 'ENOENT') {
                    throw err;
                }
            });
    })
        .return(moduleConfig);
};

var getTextBetween = function(text, startToken, endToken) {
    var start = text.indexOf(startToken);
    var end = text.indexOf(endToken);
    if (start == -1 || end == -1) {
        return "";
    }
    var offset = text.substring(start).indexOf("\n") + 1;
    var afterStart = start + offset;
    return text.substring(afterStart, end);
};

var replaceTextBetween = function(text, startToken, endToken, replaceText) {
    var newText = "";
    var start = text.indexOf(startToken);
    var end = text.indexOf(endToken);
    if (start == -1 || end == -1) {
        return text;
    }
    var offset = text.substring(start).indexOf("\n") + 1;
    var afterStart = start + offset;
    newText += text.substring(0, afterStart);
    newText += replaceText;
    newText += text.substring(end);
    if(replaceText.includes('google-services')) {
        logger.log('newText:', newText);
    }
    return newText;
};

function injectAppLinks(android_manifest, app) {
    var manifestXml = path.join(__dirname, "gradleops/"+app.manifest.shortName+"/app/src/main",  'AndroidManifest.xml');
    var app_links = android_manifest.app_links;
    var template = '<data android:host="curr_host" android:scheme="curr_scheme"/>';
    var result = '';

    if (!app_links || app_links.length === 0) {
        return Promise.resolve(true);
    }

    app_links.forEach(function (curr) {
        var curr_data = '';
        var host = curr.host;
        var scheme = curr.scheme || 'http';

        curr_data = template.replace('curr_host', host);
        curr_data = curr_data.replace('curr_scheme', scheme);
        ['path', 'pathPrefix', 'pathPattern', 'port'].forEach(function (prop) {
            if (curr[prop]) {
                curr_data = curr_data.replace('/>', ' android:'.concat(prop, '="', curr[prop], '"/>'))
            }
        });
        result = result.concat(curr_data);
    });
    result = result.concat('\n');

    return Promise.resolve()
        .then(function () {
            return fs.readFileAsync(manifestXml, 'utf-8');
        })
        .then(function (xml) {
            var XML_START_PLUGINS_LINKS = '<!--START_PLUGINS_LINKS-->';
            var XML_END_PLUGINS_LINKS = '<!--END_PLUGINS_LINKS-->';

            xml = replaceTextBetween(xml, XML_START_PLUGINS_LINKS, XML_END_PLUGINS_LINKS, result);

            return fs.writeFileAsync(manifestXml, xml, 'utf-8');
        });
};



//todo 1 find why it was changed but works from devkit.native.launchClient to gc.native.launchClient in gradleops/AndroidSeed/tealeaf/src/main/AndroidManifest.xml


function injectPluginXML(opts, app) {
    var moduleConfig = opts.moduleConfig;
    //var outputPath = opts.outputPath;
    var manifestXml =  path.join(__dirname, "gradleops", app.manifest.shortName,"app/src/main",  'AndroidManifest.xml');
    var gradleTealeafBuildFile =  path.join(__dirname, "gradleops", app.manifest.shortName, "tealeaf", 'build.gradle');
    var gradleAppBuildFile =  path.join(__dirname, "gradleops", app.manifest.shortName, "app", 'build.gradle');
    var gradleClasspathMainBuildFile =  path.join(__dirname, "gradleops", app.manifest.shortName, 'build.gradle');


    var readPluginXMLFiles = Object.keys(moduleConfig).map(function (moduleName) {
        var injectionXML = moduleConfig[moduleName].config.injectionXML;

        if (injectionXML) {
            var filepath = path.join(moduleConfig[moduleName].path, 'android', injectionXML);
            logger.log('Reading plugin XML:', filepath);

            return fs.readFileAsync(filepath, 'utf-8');
        }
    });

    var readTealeafGradleDevkitPluginsDependenciesXMLFiles = Object.keys(moduleConfig).map(function (moduleName) {
        var injectionTealeafGradleXML = moduleConfig[moduleName].config.injectionTealeafModuleGradleXML;

        if (injectionTealeafGradleXML) {
            var filepath = path.join(moduleConfig[moduleName].path, 'android', injectionTealeafGradleXML);
            logger.log('Reading injectionTealeafModuleGradleXML Gradle XML:', filepath);

            return fs.readFileAsync(filepath, 'utf-8');
        }
    });

    var readAppGradleManifestPlaceholdersXMLFiles = Object.keys(moduleConfig).map(function (moduleName) {
        var injectionAppGradleXML = moduleConfig[moduleName].config.injectionManifestPlaceholdersGradleXML;

        if (injectionAppGradleXML) {
            var filepath = path.join(moduleConfig[moduleName].path, 'android', injectionAppGradleXML);
            logger.log('Reading Tealeaf Gradle XML:', filepath);

            return fs.readFileAsync(filepath, 'utf-8');
        }
    });

    var readAppGradleDevkitPluginsDependenciesXMLFiles = Object.keys(moduleConfig).map(function (moduleName) {
        var injectionAppGradleXML = moduleConfig[moduleName].config.injectionAppModuleGradleXML;

        if (injectionAppGradleXML) {
            var filepath = path.join(moduleConfig[moduleName].path, 'android', injectionAppGradleXML);
            logger.log('Reading Tealeaf Gradle XML:', filepath);

            return fs.readFileAsync(filepath, 'utf-8');
        }
    });

    var readAppGradleAndroidPluginsXMLFiles = Object.keys(moduleConfig).map(function (moduleName) {
        var injectionAppGradleXML = moduleConfig[moduleName].config.injectionAppModuleGradleAndroidPluginsGradleXML;

        if (injectionAppGradleXML) {
            var filepath = path.join(moduleConfig[moduleName].path, 'android', injectionAppGradleXML);
            logger.log('Reading App  Gradle XML:', filepath);

            return fs.readFileAsync(filepath, 'utf-8');
        }
    });

    var readTealeafGradleAndroidPluginsXMLFiles = Object.keys(moduleConfig).map(function (moduleName) {
        var injectionTealeafGradleXML = moduleConfig[moduleName].config.injectionTealeafModuleGradleAndroidPluginsGradleXML;

        if (injectionTealeafGradleXML) {
            var filepath = path.join(moduleConfig[moduleName].path, 'android', injectionTealeafGradleXML);
            logger.log('Reading App  Gradle XML:', filepath);

            return fs.readFileAsync(filepath, 'utf-8');
        }
    });



    var readGradleClasspathAndroidPluginsXMLFiles = Object.keys(moduleConfig).map(function (moduleName) {
        var injectionMainGradleXML = moduleConfig[moduleName].config.injectionGradleClasspathXML;

        if (injectionMainGradleXML) {
            var filepath = path.join(moduleConfig[moduleName].path, 'android', injectionMainGradleXML);
            logger.log('Reading Main Gradle XML:', filepath);

            return fs.readFileAsync(filepath, 'utf-8');
        }
    });

// TODO update devkit howto `## using and setting up android plugins`
// update devkit readme to use export JAVA_HOME=`/usr/libexec/java_home -v 1.8`

    return Promise.all([
        fs.readFileAsync(manifestXml, 'utf-8')
    ].concat(readPluginXMLFiles))
        .then(function (results) {
            var xml = results.shift();
            // TODO: don't use regular expressions

            if (results && results.length > 0 && xml && xml.length > 0) {
                var XML_START_PLUGINS_MANIFEST = '<!--START_PLUGINS_MANIFEST-->';
                var XML_END_PLUGINS_MANIFEST = '<!--END_PLUGINS_MANIFEST-->';
                var XML_START_PLUGINS_ACTIVITY = '<!--START_PLUGINS_ACTIVITY-->';
                var XML_END_PLUGINS_ACTIVITY = '<!--END_PLUGINS_ACTIVITY-->';
                var XML_START_PLUGINS_APPLICATION = '<!--START_PLUGINS_APPLICATION-->';
                var XML_END_PLUGINS_APPLICATION = '<!--END_PLUGINS_APPLICATION-->';

                var manifestXmlManifestStr = '';
                var manifestXmlActivityStr = '';
                var manifestXmlApplicationStr = '';

                for (var i = 0; i < results.length; ++i) {
                    var pluginXml = results[i];
                    if (!pluginXml) { continue; }

                    manifestXmlManifestStr += getTextBetween(pluginXml, XML_START_PLUGINS_MANIFEST, XML_END_PLUGINS_MANIFEST);
                    manifestXmlActivityStr += getTextBetween(pluginXml, XML_START_PLUGINS_ACTIVITY, XML_END_PLUGINS_ACTIVITY);
                    manifestXmlApplicationStr += getTextBetween(pluginXml, XML_START_PLUGINS_APPLICATION, XML_END_PLUGINS_APPLICATION);
                }

                xml = replaceTextBetween(xml, XML_START_PLUGINS_MANIFEST, XML_END_PLUGINS_MANIFEST, manifestXmlManifestStr);
                xml = replaceTextBetween(xml, XML_START_PLUGINS_ACTIVITY, XML_END_PLUGINS_ACTIVITY, manifestXmlActivityStr);
                xml = replaceTextBetween(xml, XML_START_PLUGINS_APPLICATION, XML_END_PLUGINS_APPLICATION, manifestXmlApplicationStr);
                return fs.writeFileAsync(manifestXml, xml, 'utf-8');
            } else {
                logger.log('No plugin XML to inject');
            }
        })
        // read and apply plugins to tealeaf build.gradle
        .then(function () {

            return Promise.all([
                fs.readFileAsync(gradleTealeafBuildFile, 'utf-8')]
                .concat(readTealeafGradleDevkitPluginsDependenciesXMLFiles)
            )

        .then(function (results) {
            var xml = results.shift();
            if (results && results.length > 0 && xml && xml.length > 0) {
                var tealeafGradleBuildStr = '';
                var XML_START_PLUGINS_DEPENDENCIES =  '//<!--START_PLUGINS_DEPENDENCIES-->';
                var XML_END_PLUGINS_DEPENDENCIES = '//<!--END_PLUGINS_DEPENDENCIES-->';

                for (var i = 0; i < results.length; ++i) {
                    var gradleXml = results[i];
                    if (!gradleXml) { continue; }

                    tealeafGradleBuildStr += getTextBetween(gradleXml, XML_START_PLUGINS_DEPENDENCIES, XML_END_PLUGINS_DEPENDENCIES );
                    // logger.log('tealeafGradleBuildStr: '+tealeafGradleBuildStr);
                }

                xml = replaceTextBetween(xml, XML_START_PLUGINS_DEPENDENCIES, XML_END_PLUGINS_DEPENDENCIES , tealeafGradleBuildStr);

                return fs.writeFileAsync(gradleTealeafBuildFile, xml, 'utf-8');
            } else {
                logger.log('No plugin gradle dependency to inject');
            }
        })
        })
        // read and apply manifest placeholders to app build.gradle
        .then(function () {

            return Promise.all([
                fs.readFileAsync(gradleAppBuildFile, 'utf-8')]
                .concat(readAppGradleManifestPlaceholdersXMLFiles)
            )

        .then(function (results) {
            var xml = results.shift();
            if (results && results.length > 0 && xml && xml.length > 0) {
                var tealeafGradleBuildStr = '';
                var XML_START_MANIFEST_PLACEHOLDERS =  '//<!--START_MANIFEST_PLACEHOLDERS-->';
                var XML_END_MANIFEST_PLACEHOLDERS = '//<!--END_MANIFEST_PLACEHOLDERS-->';

                for (var i = 0; i < results.length; ++i) {
                    var gradleXml = results[i];
                    if (!gradleXml) { continue; }


                    var textBetween = getTextBetween(gradleXml, XML_START_MANIFEST_PLACEHOLDERS, XML_END_MANIFEST_PLACEHOLDERS);
                    let textBetweenLower = textBetween.toLowerCase();
                    if(textBetweenLower.includes('onesignal')){
                        let onesignalAppID = app.manifest.android.onesignalAppID;
                        logger.log('onesignalAppID1: ', onesignalAppID);
                        textBetween = textBetween.replace('onesignalAppID', onesignalAppID)
                    }

                    tealeafGradleBuildStr += textBetween

                }

                xml = replaceTextBetween(xml, XML_START_MANIFEST_PLACEHOLDERS, XML_END_MANIFEST_PLACEHOLDERS , tealeafGradleBuildStr);

                return fs.writeFileAsync(gradleAppBuildFile, xml, 'utf-8');
            } else {
                logger.log('No plugin gradle dependency to inject');
            }
        })
        })
        // read and apply manifest placeholders to tealeaf build.gradle
        .then(function () {

            return Promise.all([
                fs.readFileAsync(gradleTealeafBuildFile, 'utf-8')]
                .concat(readAppGradleManifestPlaceholdersXMLFiles)
            )

        .then(function (results) {
            var xml = results.shift();
            if (results && results.length > 0 && xml && xml.length > 0) {
                var tealeafGradleBuildStr = '';
                var XML_START_MANIFEST_PLACEHOLDERS =  '//<!--START_MANIFEST_PLACEHOLDERS-->';
                var XML_END_MANIFEST_PLACEHOLDERS = '//<!--END_MANIFEST_PLACEHOLDERS-->';

                for (var i = 0; i < results.length; ++i) {
                    var gradleXml = results[i];
                    if (!gradleXml) { continue; }


                    var textBetween = getTextBetween(gradleXml, XML_START_MANIFEST_PLACEHOLDERS, XML_END_MANIFEST_PLACEHOLDERS);
                    let textBetweenLower = textBetween.toLowerCase();
                    if(textBetweenLower.includes('onesignal')){
                        let onesignalAppID = app.manifest.android.onesignalAppID;
                        logger.log('onesignalAppID1: ', onesignalAppID);
                        textBetween = textBetween.replace('onesignalAppID', onesignalAppID)
                    }

                    tealeafGradleBuildStr += textBetween
                }

                xml = replaceTextBetween(xml, XML_START_MANIFEST_PLACEHOLDERS, XML_END_MANIFEST_PLACEHOLDERS , tealeafGradleBuildStr);

                return fs.writeFileAsync(gradleTealeafBuildFile, xml, 'utf-8');
            } else {
                logger.log('No plugin gradle dependency to inject');
            }
        })
        })
        // read and apply plugins to tealeaf build.gradle
        .then(function () {

            return Promise.all([
                fs.readFileAsync(gradleAppBuildFile, 'utf-8')]
                .concat(readAppGradleDevkitPluginsDependenciesXMLFiles)
            )

        .then(function (results) {
            var xml = results.shift();
            if (results && results.length > 0 && xml && xml.length > 0) {
                var tealeafGradleBuildStr = '';
                var XML_START_PLUGINS_DEPENDENCIES =  '//<!--START_PLUGINS_DEPENDENCIES-->';
                var XML_END_PLUGINS_DEPENDENCIES = '//<!--END_PLUGINS_DEPENDENCIES-->';

                for (var i = 0; i < results.length; ++i) {
                    var gradleXml = results[i];
                    if (!gradleXml) { continue; }

                    tealeafGradleBuildStr += getTextBetween(gradleXml, XML_START_PLUGINS_DEPENDENCIES, XML_END_PLUGINS_DEPENDENCIES );
                    // logger.log('tealeafGradleBuildStr: '+tealeafGradleBuildStr);
                }

                xml = replaceTextBetween(xml, XML_START_PLUGINS_DEPENDENCIES, XML_END_PLUGINS_DEPENDENCIES , tealeafGradleBuildStr);

                return fs.writeFileAsync(gradleAppBuildFile, xml, 'utf-8');
            } else {
                logger.log('No plugin gradle dependency to inject');
            }
        })
        })
        // read and apply plugins to app build.gradle (mainly to integrate Google Play Services plugin)
        .then(function () {

            return Promise.all([
                fs.readFileAsync(gradleAppBuildFile, 'utf-8')]
                .concat(readAppGradleAndroidPluginsXMLFiles)
            )

        .then(function (results) {
            var xml = results.shift();
            if (results && results.length > 0 && xml && xml.length > 0) {
                var appGradleBuildStr = '';
                var XML_START_ANDROID_PLUGINS =  '//<!--START_ANDROID_PLUGINS-->';
                var XML_END_ANDROID_PLUGINS = '//<!--END_ANDROID_PLUGINS-->';

                for (var i = 0; i < results.length; ++i) {
                    var gradleXml = results[i];
                    if (!gradleXml) {
                        continue;
                    }

                    appGradleBuildStr += getTextBetween(gradleXml, XML_START_ANDROID_PLUGINS, XML_END_ANDROID_PLUGINS );
                    //logger.log('tealeafGradleBuildStr: '+tealeafGradleBuildStr);
                }

                xml = replaceTextBetween(xml, XML_START_ANDROID_PLUGINS, XML_END_ANDROID_PLUGINS , appGradleBuildStr);

                return fs.writeFileAsync(gradleAppBuildFile, xml, 'utf-8');
            } else {
                logger.log('No plugin gradle dependency to inject');
            }
        })
        })
        // read and apply plugins to ** tealeaf ** build.gradle (mainly to integrate Google Play Services plugin)
        .then(function () {

            return Promise.all([
                fs.readFileAsync(gradleTealeafBuildFile, 'utf-8')]
                .concat(readTealeafGradleAndroidPluginsXMLFiles)
            )

        .then(function (results) {
            var xml = results.shift();
            if (results && results.length > 0 && xml && xml.length > 0) {
                var appGradleBuildStr = '';
                var XML_START_ANDROID_PLUGINS =  '//<!--START_ANDROID_PLUGINS-->';
                var XML_END_ANDROID_PLUGINS = '//<!--END_ANDROID_PLUGINS-->';

                for (var i = 0; i < results.length; ++i) {
                    var gradleXml = results[i];
                    if (!gradleXml) {
                        continue;
                    }

                    appGradleBuildStr += getTextBetween(gradleXml, XML_START_ANDROID_PLUGINS, XML_END_ANDROID_PLUGINS );
                    //logger.log('tealeafGradleBuildStr: '+tealeafGradleBuildStr);
                }

                xml = replaceTextBetween(xml, XML_START_ANDROID_PLUGINS, XML_END_ANDROID_PLUGINS , appGradleBuildStr);

                return fs.writeFileAsync(gradleTealeafBuildFile, xml, 'utf-8');
            } else {
                logger.log('No plugin gradle dependency to inject');
            }
        })
        })
        // read and apply plugins to main build.gradle (mainly to integrate Google Play Services plugin)
        .then(function () {

            return Promise.all([
                fs.readFileAsync(gradleClasspathMainBuildFile, 'utf-8')]
                .concat(readGradleClasspathAndroidPluginsXMLFiles)
            )

        .then(function (results) {
            var xml = results.shift();
            if (results && results.length > 0 && xml && xml.length > 0) {
                var mainGradleBuildStr = '';
                var XML_START_GOOGLE_PLAY_PLUGINS_CLASSPATH =  '//<!--START_GOOGLE_PLAY_PLUGINS_CLASSPATH-->';
                var XML_END_GOOGLE_PLAY_PLUGINS_CLASSPATH = '//<!--END_GOOGLE_PLAY_PLUGINS_CLASSPATH-->';

                for (var i = 0; i < results.length; ++i) {
                    var gradleXml = results[i];
                    if (!gradleXml) { continue; }

                    mainGradleBuildStr += getTextBetween(gradleXml, XML_START_GOOGLE_PLAY_PLUGINS_CLASSPATH, XML_END_GOOGLE_PLAY_PLUGINS_CLASSPATH );
                    // logger.log('tealeafGradleBuildStr: '+tealeafGradleBuildStr);
                }
                logger.log("mainGradleBuildStr: "+ mainGradleBuildStr);
                xml = replaceTextBetween(xml, XML_START_GOOGLE_PLAY_PLUGINS_CLASSPATH, XML_END_GOOGLE_PLAY_PLUGINS_CLASSPATH , mainGradleBuildStr);

                return fs.writeFileAsync(gradleClasspathMainBuildFile, xml, 'utf-8');
            } else {
                logger.log('No plugin gradle dependency to inject');
            }
        })
        .then (function () {
                return installJarsDependencies(app)
            })
        });
}

// TODO
/*

1 how devkit onesignal uses gcm in manifest and code (what are additional services)
2 migrate from jars to gradle
        ironsource
        gameplay (just check if it is possible to remove arch-android and replace it with implementation)
        crashlytics (much work)
*/


// do not remove now
var installModuleCode = function (api, app, opts) {
    var moduleConfig = opts.moduleConfig;
    var outputPath = opts.outputPath;

    function handleFile(baseDir, filePath, replacer) {
        var ext = path.extname(filePath);
        if (ext == '.java' || ext === '.aidl') {
            return fs.readFileAsync(path.join(baseDir, filePath), 'utf-8')
                .then(function (contents) {
                    var pkgName = contents.match(/(package[\s]+)([a-z.A-Z0-9]+)/g)[0].split(' ')[1];
                    var pkgDir = pkgName.replace(/\./g, "/");
                    var outFile = path.join(__dirname, "gradleops", app.manifest.shortName, "tealeaf/src/main/", ext.substr(1), pkgDir, path.basename(filePath));

                    logger.log("Installing Java package", pkgName, "to", outFile);

                    // Run injectionSource section of associated module
                    if (replacer && replacer.length > 0) {
                        for (var jj = 0; jj < replacer.length; ++jj) {
                            var findString = replacer[jj].regex;
                            var keyForReplace = replacer[jj].keyForReplace;
                            var replaceString = app.manifest.android[keyForReplace];
                            if (replaceString) {
                                logger.log(" - Running find-replace for", findString, "->", replaceString, "(android:", keyForReplace + ")");
                                var rexp = new RegExp(findString, "g");
                                contents = contents.replace(rexp, replaceString);
                            } else {
                                logger.error(" - Unable to find android key for", keyForReplace);
                            }
                        }
                    }

                    return fs.outputFileAsync(outFile, contents, 'utf-8');
                });
        } else if (ext == '.so') {
            var src = path.join(baseDir, filePath);
            var basename = path.basename(filePath);
            return Promise.all([
                // remove armeabi because armeabi-v7a is enough
                fs.copyAsync(src, path.join(outputPath, "tealeaf/src/main", 'libs', 'armeabi', basename)),
                fs.copyAsync(src, path.join(outputPath, "tealeaf/src/main", 'libs', 'armeabi-v7a', basename))
            ]);
        } else {
            return fs.copyAsync(path.join(baseDir, filePath), path.join(outputPath, "tealeaf/src/main", filePath));
        }
    }
// Don't need this seems gradle has been set up to read all libraries in libs directory, just need to copy jars
    /* function installLibraries(libraries) {
       if (!libraries || !libraries.length) { return; }

       var projectPropertiesFile = path.join(__dirname+"/gradleops/"+app.manifest.shortName, "project.properties");

       var libraryFiles = libraries.map(function (library) {
         return path.join(modulePath, 'android', library);
       });

       return Promise.map(libraryFiles, function (libraryFile) {
           // Set up library build files
           logger.log("Installing module library:", libraryFile);
           return spawnWithLogger(api, 'android', [
               "update", "project", "--target", ANDROID_TARGET,
               "--path", libraryFile
             ]);
         })
         .then(function () {
           return fs.readFileAsync(projectPropertiesFile, "utf-8");
         })
         .then(function (properties) {
           // find the largest library reference number
           var libraryNumber = 1;
           properties.replace(/^[^#]+android\.library\.reference\.(\d+).*$/gm, function (line, ref) {
             // for each (uncommented) line with a library reference
             ref = parseInt(ref);
             if (ref >= libraryNumber) {
               libraryNumber = ref + 1;
             }
           });

           var libraryReferences = '\n' + libraryFiles.map(function (library) {
               var key = 'android.library.reference.' + (libraryNumber++);
               var value = path.relative(outputPath, library);
               return key + '=' + value;
             })
             .join('\n');

           return fs.appendFileAsync(projectPropertiesFile, libraryReferences);
         });
     }*/

    function installJar(jarFile) {
        logger.log("Installing module JAR:", jarFile);
        var jarDestPath = path.join(__dirname+"/gradleops/"+app.manifest.shortName, "tealeaf/libs", path.basename(jarFile));
        logger.log("Installing JAR file:", jarDestPath);
        return fs.unlinkAsync(jarDestPath)
            .catch(function () {})
            .then(function () {
                return fs.copy(jarFile, jarDestPath, 'junction');
            });
    }

    var tasks = [];


    for (var moduleName in moduleConfig) {
        var config = moduleConfig[moduleName].config;
        var modulePath = moduleConfig[moduleName].path;

        config.copyFiles && config.copyFiles.forEach(function (filename) {
            tasks.push(handleFile(path.join(modulePath, 'android'), filename, config.injectionSource));
        });

        config.copyCustomFiles && config.copyCustomFiles.forEach(function (customfile) {
            tasks.push(fs.copyAsync(path.join(modulePath, 'android', customfile.file),
                path.join(__dirname, "gradleops", app.manifest.shortName, customfile.path, customfile.file)));
        });

        config.copyGameFiles && config.copyGameFiles.forEach(function (filename) {
            tasks.push(handleFile(app.paths.root, filename, config.injectionSource));
        });

        config.jars && config.jars.forEach(function (jar) {
            tasks.push(installJar(path.join(modulePath, 'android', jar)));
        });



        // don't need this, see line 295 `function installLibraries(`
        // tasks.push(installLibraries(config.libraries));
    }

    return Promise.all(tasks);
};


/** install jar dependencies from libs folder where jars and aars previously copied to*/
function installJarsDependencies(app) {


    var gradleBuildFile =  path.join(__dirname, "gradleops", app.manifest.shortName,"tealeaf",  'build.gradle');

    return fs.readFileAsync(gradleBuildFile, 'utf-8')
        .then(function (gradleBuildFileData) {

            var XML_START_PLUGINS_BULK_DEPENDENCIES = '//<!--START_PLUGINS_BULK_DEPENDENCIES-->';
            var XML_END_PLUGINS_BULK_DEPENDENCIES = '//<!--END_PLUGINS_BULK_DEPENDENCIES-->';

            var archivesDependencies ='';

            var gradleJarDependencyStr =''
            gradleJarDependencyStr = getTextBetween(gradleBuildFileData, XML_START_PLUGINS_BULK_DEPENDENCIES, XML_END_PLUGINS_BULK_DEPENDENCIES);

            const libsDir = path.join(__dirname, "gradleops", app.manifest.shortName,"tealeaf",  'libs');
            const fsNotExtra = require('fs');
            const files = fs.readdirSync(libsDir);
                files.forEach(function (jar) {
                logger.log("Installing JARs in gradle:", jar);
                archivesDependencies +=  "implementation files('libs/"+path.basename(jar)+"')" + "\n"
            });

            gradleJarDependencyStr +=   gradleJarDependencyStr + "\n"+ archivesDependencies ;

            gradleBuildFileData = replaceTextBetween(gradleBuildFileData, XML_START_PLUGINS_BULK_DEPENDENCIES, XML_END_PLUGINS_BULK_DEPENDENCIES, gradleJarDependencyStr);
            return fs.writeFileAsync(gradleBuildFile, gradleBuildFileData , 'utf-8');

        })
}

//// Utilities

function transformXSL(api, inFile, outFile, xslFile, params, config) {
    for (var key in params) {
        if (typeof params[key] !== 'string') {
            if (!params[key] || typeof params[key] === 'object') {
                logger.error("settings for AndroidManifest: value for", chalk.yellow(key), "is not a string");
            }


            params[key] = JSON.stringify(params[key]);
        }
    }

    params['package'] = config.packageName

    var outFileTemp = outFile + ".temp";
    return new Promise(function (resolve, reject) {
        api.jvmtools.exec({
            tool: 'xslt',
            args: [
                "--in", inFile,
                "--out", outFileTemp,
                "--stylesheet", xslFile,
                "--params", JSON.stringify(params)
            ]
        }, function (err, xslt) {
            if (err) { return reject(err); }

            var logger = api.logging.get('xslt');
            xslt.on('out', logger.out);
            xslt.on('err', logger.err);
            xslt.on('end', resolve);
        });
    })
        .then(function () {
            return fs.readFileAsync(outFileTemp, 'utf-8');
        })
        .then(function(contents) {
            contents = contents.replace(/android:label=\"[^\"]*\"/g, "android:label=\""+params.title+"\"");
            fs.writeFile(outFile, contents, 'utf-8');
        });
}
/* Seems this is not required with gradle
function buildSupportProjects(api, config) {
  var rootDir = __dirname;
  var tealeafDir = path.join(__dirname, 'AndroidSeed');
  var singleArch = config.argv.arch;
  return Promise.try(function () {
      if (config.argv.clean || singleArch) {
        return spawnWithLogger(api, 'make', ['clean'], {cwd: rootDir});
      }
    })
    .then(function () {
      var args = ["-j", "8", (config.debug ? "DEBUG=1" : "RELEASE=1")];
      if (singleArch) {
        args.push('APP_ABI=' + singleArch);
      }

      return spawnWithLogger(api, 'ndk-build', args , {cwd: tealeafDir});
    });
}*/


// not tested because not used now, will get back to it later
function saveLocalizedStringsXmls(outputPath, titles) {
    var stringsXmlPath = path.join(outputPath, "app/src/main", "res/values/strings.xml");
    var stringsXml = fs.readFileSync(stringsXmlPath, "utf-8");
    return Promise.map(Object.keys(titles), function (lang) {
        var title = titles[lang];
        var i = stringsXml.indexOf('</resources>');
        var first = stringsXml.substring(0, i);
        var second = stringsXml.substring(i);
        var inner = '<string name="title">' + title + '</string>';
        var finalXml = first + inner + second;
        var values = lang == 'en' ? 'values' : 'values-' + lang;
        var stringsFile = path.join(outputPath, "app/src/main",'res', values, 'strings.xml');
        return fs.outputFileAsync(stringsFile, finalXml, 'utf-8');
    });
}

function executeOnCreate(api, app, config, opts) {
    var modules = app.modules;
    var hookName = 'onCreateProject';

    return Promise.resolve(Object.keys(modules))
        .map(function (moduleName) {
            var module = modules[moduleName];
            var buildExtension = module.extensions && module.extensions.build;

            buildExtension = buildExtension ? require(buildExtension) : null;

            if (!buildExtension || !buildExtension[hookName]) {
                return;
            }

            return new Promise(function (resolve, reject) {
                var retVal = buildExtension[hookName](api, app, config, function (err, res) {
                    if (err) {
                        reject(err);
                    } else {
                        resolve(res);
                    }
                });

                if (retVal) { resolve(retVal); }
            })
        });
}

function makeAndroidProject(api, app, config, opts) {
    var projectPropertiesFile = path.join(__dirname+"/gradleops/"+app.manifest.shortName, 'project.properties');
    return fs.unlinkAsync(projectPropertiesFile)
        .catch(function () {}) // ignore error if file doesn't exist
        .then(function () {
            return spawnWithLogger(api, 'pwd', ["-L"]);
        })
        .then(function () {
            return spawnWithLogger(api, 'printf',  [__dirname])
        })
        .then(function () {
            return spawnWithLogger(api, 'rm', ["-rf",__dirname+"/gradleops/"+app.manifest.shortName])
        })
        // call gradle project to copy seed
        .then(function () {
            return spawnWithLogger(api, 'bash', [
                // script path which copies gradle seed project to new named project
                "./template",
                // new name and location
                app.manifest.shortName,
                // template name and location
                "AndroidSeed",
                // new package
                config.packageName
            ], {cwd: './modules/devkit-core/modules/native-android/gradleops/'})
        })
        .then(function () {
            return spawnWithLogger(api, 'pwd', ["-L"]);
        })
        // Clean gradle projects
        .then(function () {
            return spawnWithLogger(api, './gradlew', [
                "clean"
            ], {cwd: __dirname+"/gradleops/"+app.manifest.shortName});
        })
        .then(function () {
            return spawnWithLogger(api, 'pwd', ["-L"]);
        })
        // make new package dir
        .then(function () {
            return spawnWithLogger(api, 'mkdir', ["-p", path.join(__dirname+"/gradleops/"+app.manifest.shortName,
                "app/src/main/java",
                config.packageName.split('.').join('/'))]);
        })
        // Rename activity
        .then(function () {
            var activityFileOld = path.join(__dirname+"/gradleops/"+app.manifest.shortName,
                "app/src/main/java",
                "com", app.manifest.shortName,
                "AndroidSeedActivity" + ".java");
            var activityFileNew = path.join(__dirname+"/gradleops/"+app.manifest.shortName,
                "app/src/main/java",
                config.packageName.split('.').join('/'),
                app.manifest.shortName + "Activity.java");
            return spawnWithLogger(api, 'mv', [activityFileOld,activityFileNew]);
        })
        .then(function () {
            var dexDir = '\nout.dexed.absolute.dir=../.dex/\nsource.dir=src\n';
            return [
                saveLocalizedStringsXmls( __dirname+"/gradleops/"+app.manifest.shortName, config.titles),
                updateManifest(api, app, config, opts),
                updateActivity(app, config, opts),
                executeOnCreate(api, app, config, opts)
            ];
        })
        .all();
}

// Todo test and debug sign apk path and command
function signAPK(api, shortName, outputPath, debug) {
    var signArgs, alignArgs;
    var binDir = path.join(outputPath, "bin");

    logger.log('Signing APK at', binDir);
    if (debug) {
        var keyPath = path.join(process.env['HOME'], '.android', 'debug.keystore');
        signArgs = [
            "-sigalg", "MD5withRSA", "-digestalg", "SHA1",
            "-keystore", keyPath, "-storepass", "android",
            "-signedjar", shortName + "-debug-unaligned.apk",
            shortName + "-debug.apk", "androiddebugkey"
        ];
        alignArgs = [
            "-f", "-v", "4", shortName + "-debug-unaligned.apk", shortName + "-debug.apk"
        ];
    } else {
        var keystore = process.env['DEVKIT_ANDROID_KEYSTORE'];
        if (!keystore) { throw new BuildError('missing environment variable DEVKIT_ANDROID_KEYSTORE'); }

        var storepass = process.env['DEVKIT_ANDROID_STOREPASS'];
        if (!storepass) { throw new BuildError('missing environment variable DEVKIT_ANDROID_STOREPASS'); }

        var keypass = process.env['DEVKIT_ANDROID_KEYPASS'];
        if (!keypass) { throw new BuildError('missing environment variable DEVKIT_ANDROID_KEYPASS'); }

        var key = process.env['DEVKIT_ANDROID_KEY'];
        if (!key) { throw new BuildError('missing environment variable DEVKIT_ANDROID_KEY'); }

        signArgs = [
            "-sigalg", "MD5withRSA", "-digestalg", "SHA1",
            "-keystore", keystore, "-storepass", storepass, "-keypass", keypass,
            "-signedjar", shortName + "-unaligned.apk",
            shortName + "-release-unsigned.apk", key
        ];

        alignArgs = [
            "-f", "-v", "4", shortName + "-unaligned.apk", shortName + "-aligned.apk"
        ];
    }

    return spawnWithLogger(api, 'jarsigner', signArgs, {cwd: binDir})
        .then(function () {
            return spawnWithLogger(api, 'zipalign', alignArgs , {cwd: binDir});
        });
}
// seems this is not needed
function repackAPK(api, outputPath, apkName, cb) {
    var apkPath = path.join('bin', apkName);
    spawnWithLogger(api, 'zip', [apkPath, '-d', 'META-INF/*'], {cwd: outputPath}, function (err) {
        if (err) { return cb(err); }
        spawnWithLogger(api, 'zip', [apkPath, '-u'], {cwd: outputPath}, cb);
    });
}


function copyAssets(app, project, destPath) {
    var assetsPath = project.manifest.assets || [];

    // Fixed for gradle structure
    return Promise.map(assetsPath, function(asset) {
        logger.log('Copying', asset, 'to ' + path.join(destPath, asset));
        return fs.copyAsync(asset, path.join(destPath, asset));
    });
}


// Todo later: removed this function and copy directly to required path without intermediate copying to outdated output path
function copyJSAssets(app, project, destPath, config) {
    var jsResourcesAssetsPath = path.resolve( config.outputPath, "assets/resources" );
    return fs.copyAsync(jsResourcesAssetsPath, destPath+"/resources");
}

function copyIcons(app, outputPath) {
    return Promise.all([
        copyIcon(app, outputPath, "l", "36"),
        copyIcon(app, outputPath, "m", "48"),
        copyIcon(app, outputPath, "h", "72"),
        copyIcon(app, outputPath, "xh", "96"),
        copyIcon(app, outputPath, "xxh", "144"),
        copyIcon(app, outputPath, "xxxh", "192"),
        copyRoundIcon(app, outputPath, "l", "36"),
        copyRoundIcon(app, outputPath, "m", "48"),
        copyRoundIcon(app, outputPath, "h", "72"),
        copyRoundIcon(app, outputPath, "xh", "96"),
        copyRoundIcon(app, outputPath, "xxh", "144"),
        copyRoundIcon(app, outputPath, "xxxh", "192"),
        copyNotifyIcon(app, outputPath, "l", "low"),
        copyNotifyIcon(app, outputPath, "m", "med"),
        copyNotifyIcon(app, outputPath, "h", "high"),
        copyNotifyIcon(app, outputPath, "xh", "xhigh"),
        copyNotifyIcon(app, outputPath, "xxh", "xxhigh"),
        copyNotifyIcon(app, outputPath, "xxxh", "xxxhigh"),
        copyShortcutIcons(app, outputPath, "l", "36"),
        copyShortcutIcons(app, outputPath, "m", "48"),
        copyShortcutIcons(app, outputPath, "h", "72"),
        copyShortcutIcons(app, outputPath, "xh", "96"),
        copyShortcutIcons(app, outputPath, "xxh", "144"),
        copyShortcutIcons(app, outputPath, "xxxh", "192")
    ]);
}

function copyRoundIcon(app, outputPath, tag, size) {
    var destPath = path.join(outputPath, "res/mipmap-" + tag + "dpi/round_icon.png");
    var android = app.manifest.android;
    var iconPath = android.icons && android.icons.round && android.icons.round[size];

    if (iconPath) {
        iconPath = path.resolve(app.paths.root, iconPath);
        return fs.copyAsync(iconPath, destPath);
    }

    logger.warn("No icon specified in the manifest for size '" + size + "'. Using the default icon for this size. This is probably not what you want.");
}

function copyIcon(app, outputPath, tag, size) {
    var destPath = path.join(outputPath , "res/mipmap-" + tag + "dpi/icon.png");
    var android = app.manifest.android;
    var iconPath = android.icons && android.icons[size];

    if (iconPath) {
        iconPath = path.resolve(app.paths.root, iconPath);
        return fs.copyAsync(iconPath, destPath);
    }

    logger.warn("No icon specified in the manifest for size '" + size + "'. Using the default icon for this size. This is probably not what you want.");
}

function copyNotifyIcon(app, outputPath, tag, name) {
    var destPath = path.join(outputPath, "res/drawable-" + tag + "dpi/notifyicon.png");
    var android = app.manifest.android;
    var iconPath = android.icons && android.icons.alerts && android.icons.alerts[name];

    if (iconPath) {
        return fs.copyAsync(iconPath, destPath);
    } else {
        // Do not copy a default icon to this location -- Android will fill in
        // the blanks intelligently.
        logger.warn("No alert icon specified in the manifest for density '" + name + "'");
    }
}

function copyShortcutIcons(app, outputPath, tag, name) {
    var destPath = path.join(outputPath, "res/drawable-" + tag + "dpi/");
    var android = app.manifest.android;
    var shortcutIcons = android.icons && android.icons.shortcuts && android.icons.shortcuts[name];
    var regExp = new RegExp("^.*[\\\/](.*)" + name + "(.png)");
    var targetFile = function (val, p1, p2) {
        return "shortcut_" + p1 + p2;
    };

    if (shortcutIcons) {
        return Promise.map(shortcutIcons, function (iconPath) {
            return fs.copyAsync(iconPath, destPath + iconPath.replace(regExp, targetFile));
        });
    }
}

var SPLASH_FILES = [
    'portrait480',
    'portrait960',
    'portrait1024',
    'portrait1136',
    'portrait2048',
    'portrait2960',
    'landscape768',
    'landscape1536',
    'universal'
];

var DEFAULT_SPLASH_CONFIG = {};
SPLASH_FILES.forEach(function (key) {
    DEFAULT_SPLASH_CONFIG[key] = "resources/splash/" + key + ".png";
});

function copySplash(api, app, outputDir) {
    var splashPaths = app.manifest.android.splash || app.manifest.splash || DEFAULT_SPLASH_CONFIG;
    var destPath = path.join(outputDir, 'assets/resources');
    return fs.mkdirsAsync(destPath)
        .then(function () {
            return SPLASH_FILES.map(function (key) {
                var filename = splashPaths[key];
                if (!filename) { return false; }

                return existsAsync(filename)
                    .then(function (exists) {
                        if (!exists) {
                            logger.error('Splash file (manifest.splash.' + key + ') does not',
                                'exist (' + filename + ')');
                        }

                        return {
                            key: key,
                            filename: filename,
                            exists: exists
                        };
                    });
            });
        })
        // remove files that don't exist
        .filter(function (splash) { return splash && splash.exists; })
        .map(function (splash) {
            var filename = 'splash-' + splash.key + '.png';
            var destFile = path.join(destPath, filename);
            logger.log('Copying', splash.filename, 'to "assets/resources/' + filename + '"');
            return fs.copyAsync(splash.filename, destFile);
        });
}

function copyMusic(app, outputDir) {
    if (app.manifest.splash) {
        var musicPath = app.manifest.splash.song;
        var destPath = path.join(outputDir, "res/raw", "loadingsound.mp3");
        return existsAsync(musicPath)
            .then(function (exists) {
                if (!exists) {
                    logger.warn('No valid splash music specified in the manifest (at "splash.song")');
                } else {
                    return fs.copyAsync(musicPath, destPath);
                }
            });
    }
}

function copyResDir(app, outputDir) {
    if (app.manifest.android.resDir) {
        var destPath = path.join(outputDir, "res");
        var sourcePath = path.resolve(app.manifest.android.resDir);
        return fs.copyAsync(sourcePath, destPath, {preserveTimestamps: true})
            .catch(function (e) {
                logger.warn("Could not copy your android resource dir [" + e.toString() + "]");
                throw e;
            });
    }
}

function updateManifest(api, app, config, opts) {
    var params = {
        // Empty defaults
        installShortcut: "false",

        entryPoint: "devkit.native.launchClient",
        studioName: config.studioName,

        disableLogs: config.debug ? 'false' : 'true',

        // Filled defaults
        // TODO: REMOVE ALL OF THESE FLAGS
        codeHost: "s.wee.cat",
        tcpHost: "s.wee.cat",
        codePort: "80",
        tcpPort: "4747",
        activePollTimeInSeconds: "10",
        passivePollTimeInSeconds: "20",
        syncPolling: "false",
        develop: config.debug ? 'true' : 'false',
    };

    var orientations = app.manifest.supportedOrientations;
    var orientation = "portrait";
    var otherApps = app.manifest.android.otherApps || [];

    if (orientations.indexOf("portrait") != -1 && orientations.indexOf("landscape") != -1) {
        orientation = "unspecified";
    } else if (orientations.indexOf("landscape") != -1) {
        orientation = "landscape";
    }

    function copy(target, src) {
        for (var key in src) {
            target[key] = src[key];
        }
    }

    function copyAndFlatten(target, src, prefix) {
        prefix = prefix || '';

        for (var key in src) {
            var val = src[key];
            var newPrefix = prefix.length === 0 ? key : prefix + '.' + key;
            if(typeof val === "object") {
                copyAndFlatten(target, val, newPrefix);
            } else {
                // Push to final object
                target[newPrefix] = val;
            }
        }
    }

    copy(params, app.manifest.android);
    copyAndFlatten(params, app.manifest.modules || app.manifest.addons);
    copy(params, {
        "package": config.packageName,
        title: "@string/title",
        activity: config.packageName + "." + config.activityName,
        version: "" + config.version,
        appid: app.manifest.appID.replace(PUNCTUATION_REGEX, ""), // Strip punctuation.,
        shortname: app.manifest.shortName,
        fullscreen: app.manifest.android.fullscreen,
        orientation: orientation,
        studioName: config.studioName,
        gameHash: app.manifest.version,
        sdkHash: config.sdkVersion,
        androidHash: androidVersion,
        minSdkVersion: config.argv['min-sdk-version'] || 21,
        targetSdkVersion: config.argv['target-sdk-version'] || 27,
        debuggable: config.debug ? 'true' : 'false',
        otherApps: otherApps.join('|')
    });

    // Not required because gradle project is created by copying project which includes tealeaf module installed
    // var defaultManifest = path.join(__dirname, "gradleops/AndroidSeed/tealeaf/src/main/AndroidManifest.xml");
    // var outputManifest =  path.join(__dirname, "gradleops/"+app.manifest.shortName+"/tealeaf/src/main", "AndroidManifest.xml");
    // fs.copyAsync(defaultManifest, outputManifest)
    var defaultManifest = path.join(__dirname, "gradleops/"+app.manifest.shortName+"/app/src/main", "AndroidManifest.xml");
    var outputManifest =  defaultManifest; //path.join(__dirname, "gradleops/"+app.manifest.shortName+"/tealeaf/src/main", "AndroidManifest.xml");
    injectAppLinks(app.manifest.android, app)

        .then(function () {
            return injectPluginXML(opts, app);
        })
        .then(function () {
            return Object.keys(opts.moduleConfig);
        })
        .map(function (moduleName) {
            var module = opts.moduleConfig[moduleName];
            var config = module.config;
            if (config.injectionXSL) {
                var xslPath = path.join(module.path, 'android', config.injectionXSL);
                return transformXSL(api, defaultManifest, outputManifest, xslPath, params, config);
            }
        }, {concurrency: 1}) // Run the plugin XSLT in series instead of parallel

        .then(function() {
            /** Before this copy to original file seed of mygame/manifest.json:
             copy 1: "fullscreen" : true,       // do not use strings like "true"
             copy 2:  "gameHash" : 82378912738917238,
             Otherwise you will seize following error in console
             [error]  settings for AndroidManifest: value for fullscreen is not a string
             [error]  settings for AndroidManifest: value for gameHash is not a string
             */
            // todo find place in the seed to set fields "fullscreen" and "gameHash"
            logger.log("Applying final XSL transformation");
            var xmlPath = path.join(__dirname,  "gradleops/"+app.manifest.shortName+"/app/src/main", "AndroidManifest.xml");
            return transformXSL(api, xmlPath, xmlPath,
                path.join(__dirname, "AndroidManifest.xsl"),
                params, config);
        });
}

function updateActivity(app, config, opts) {
    var activityFile = path.join(__dirname+"/gradleops/"+app.manifest.shortName,
        "app/src/main/java",
        config.packageName.split('.').join('/'),
        config.activityName + ".java");

    return fs.readFileAsync(activityFile, 'utf-8')
        .then(function (contents) {
            contents = contents
                .replace(/extends Activity/g, "extends com.tealeaf.TeaLeaf")
                .replace(/setContentView\(R\.layout\.main\);/g, "startGame();");
            return fs.writeFileAsync(activityFile, contents);
        });
}



function createProject(api, app, config) {

    var tasks = [];
    tasks.push(getModuleConfig(api, app)
        .then(function (moduleConfig) {
            return makeAndroidProject(api, app, config, {
                outputPath: config.outputPath,
                moduleConfig: moduleConfig
            })
                .return(moduleConfig);
        })
        // now it seems this is not needed but need to test of use with plugins, etc in production
        .then(function (moduleConfig) {
            return installModuleCode(api, app, {
                moduleConfig: moduleConfig,
                outputPath: config.outputPath
            });
        })
    );

    // TODO: if build switches between release to debug, clean project
    // var cleanProj = (builder.common.config.get("lastBuildWasDebug") != config.debug) || config.clean;
    // builder.common.config.set("lastBuildWasDebug", config.debug);

    // Seems not needed
    //tasks.push(buildSupportProjects(api, config));

    return Promise.all(tasks);
}

exports.build = function(api, app, config, cb) {
    logger = api.logging.get('android');

    var sdkVersion = parseFloat(config.sdkVersion);
    if (isNaN(sdkVersion) || sdkVersion < 3.1) {
        spawnWithLogger = legacySpawnWithLogger;
    }

    var argv = config.argv;

    var skipAPK = argv.apk === false;
    var skipSigning = skipAPK || argv.signing === false || config.debug;

    var shortName = app.manifest.shortName;
    if (shortName === null) {
        throw new BuildError("Build aborted: No shortName in the manifest");
    }

    var apkBuildName = "";
    if (!config.debug) {
        if (skipSigning) {
            apkBuildName = shortName + "-release-unsigned.apk";
        } else {
            apkBuildName = shortName + "-aligned.apk";
        }
    } else {
        apkBuildName = shortName + "-debug.apk";
    }

    if (!app.manifest.android) {
        logger.warn('you should add an "android" key to your app\'s manifest.json',
            'for android-specific settings');
        app.manifest.android = {};
    }

    return Promise.try(function createAndroidProjectFiles() {
        if (!config.repack) {
            return createProject(api, app, config);
        }
    })
        .then(function copyResourcesToProject() {
            return [
                // changed from config.outputPath to gradle project path
                copyIcons(app, __dirname+"/gradleops/"+app.manifest.shortName + "/app/src/main/res"),
                copyMusic(app, __dirname+"/gradleops/"+app.manifest.shortName + "/app/src/main"),
                copyResDir(app, __dirname+"/gradleops/"+app.manifest.shortName + "/app/src/main"),
                copySplash(api, app, __dirname+"/gradleops/"+app.manifest.shortName + "/app/src/main"),
                //Todo debug, unclear value of path
                copyJSAssets(api, app, __dirname+"/gradleops/"+app.manifest.shortName + "/app/src/main/assets", config),
                copyAssets(api, app, __dirname+"/gradleops/"+app.manifest.shortName + "/app/src/main/assets")
            ];
        })
        .all()

        .then(function buildAPK() {
            if (!skipAPK) {
                // try to replace repack by moving here build assemble
                /*if (config.repack) {
                  return repackAPK(api, config.outputPath, apkBuildName);
                } else {
                  var antScheme = (config.debug ? "debug" : "release");
                  return spawnWithLogger(api, 'ant', [antScheme], {
                      cwd: config.outputPath
                    });
                }*/
                // build ndk libtealeaf.so, formerly named manually libpng.so ,
                // todo make sure gitignore to fix: find out why jni/obj, jni/gen and jni/profilers were in gitignore, should they be assembled by devkit?
                return spawnWithLogger(api, 'ndk-build', [
                    "NDK_PROJECT_PATH=tealeaf/src/main",
                ], {cwd: "./modules/devkit-core/modules/native-android/gradleops/" + app.manifest.shortName})
                    .catch(BuildError, function (err) {
                        if (err.stdout && /not valid/i.test(err.stdout)) {
                            logger.log(chalk.yellow([
                                '',
                                'Android target ' + ANDROID_TARGET + ' was not available. Please ensure',
                                'you have installed the Android SDK properly, and use the',
                                '"android" tool to install API Level ' + ANDROID_TARGET.split('-')[1] + '.',
                                ''
                            ].join('\n')));
                        }

                        if (err.stdout && /no such file/i.test(err.stdout) || err.code == 126) {
                            logger.log(chalk.yellow([
                                '',
                                'You must install the Android SDK first. Please ensure the ',
                                '"android" tool is available from the command line by adding',
                                'the sdk\'s "tools/" directory to your system path.',
                                ''
                            ].join('\n')));
                        }

                        throw err;
                    })

                    // build Android project
                    .then(function () {
                        return spawnWithLogger(api, './gradlew', [
                            "build"
                            // , '--debug', '--stacktrace', // UNCOMMENT TO DEBUG
                        ], {cwd: __dirname+"/gradleops/"+app.manifest.shortName})
                            .catch(BuildError, function (err) {
                                if (err.stdout && /not valid/i.test(err.stdout)) {
                                    logger.log(chalk.yellow([
                                        '',
                                        'Android target ' + ANDROID_TARGET + ' was not available. Please ensure',
                                        'you have installed the Android SDK properly, and use the',
                                        '"android" tool to install API Level ' + ANDROID_TARGET.split('-')[1] + '.',
                                        ''
                                    ].join('\n')));
                                }

                                if (err.stdout && /no such file/i.test(err.stdout) || err.code == 126) {
                                    logger.log(chalk.yellow([
                                        '',
                                        'You must install the Android SDK first. Please ensure the ',
                                        '"android" tool is available from the command line by adding',
                                        'the sdk\'s "tools/" directory to your system path.',
                                        ''
                                    ].join('\n')));
                                }

                                throw err;
                            });
                    })

                // Todo fix moving assembled project if needed for next puposes or just deliver .apk build
                /*.then(function () {
                    //var tealeafDir = path.relative("/"+app.manifest.shortName, path.join(__dirname, "AndroidSeed"));
                    return spawnWithLogger(api, 'mv', ["/modules/devkit-core/modules/native-android/gradleops/"+app.manifest.shortName,
                        "/build/debug/native-android/"+app.manifest.shortName]);
                    //return spawnWithLogger(api, 'printf',  [opts.outputPath])
                })*/
            }
        })
        .then(function () {
            if (!skipSigning) {
                return signAPK(api, shortName, config.outputPath, config.debug);
            }
        })
        .then(function () {
            if (!skipAPK) {
                return moveAPK(api, app, config, apkBuildName)
                    .tap(function (apkPath) {
                        logger.log("built", chalk.yellow(config.packageName));
                        logger.log("saved to " + chalk.blue(apkPath));
                    })
                    .then(function (apkPath) {
                        if (argv.reveal) {
                            require('child_process').exec('open --reveal "' + apkPath + '"');
                        }

                        if (argv.install || argv.open) {
                            return installAPK(api, config, apkPath, {
                                open: !!argv.open,
                                clearStorage: argv.clearStorage
                            });
                        }
                    });
            }
        })
        .nodeify(cb);
};

function moveAPK(api, app, config, apkBuildName) {
    var shortName = app.manifest.shortName;
    var apkPath = path.join(__dirname,"gradleops",shortName,"app/build/outputs/apk/debug", "app-debug.apk");
    var destApkPath = path.join(config.outputPath, "bin", apkBuildName);
    return Promise.all([
        existsAsync(apkPath),
        fs.unlinkAsync(destApkPath)
            .catch(function () {}) // ignore if it didn't exist
    ])
        .spread(function (exists) {
            if (exists) {
                return fs.copyAsync(apkPath,destApkPath);
            } else {
                throw new BuildError("apk failed to build (missing " + destApkPath + ")");
            }
        })
        .return(destApkPath);
}

function installAPK(api, config, apkPath, opts) {
    var packageName = config.packageName;
    var activityName = config.activityName;

    function getDevices() {
        return spawnWithLogger(api, 'adb', ['devices'], {capture: true})
            .then(function (res) {
                return res.split('\n')
                    .map(function (line) {
                        return line.match(/^([0-9a-z]+)\s+(device|emulator)$/i);
                    })
                    .filter(function (match) { return match; })
                    .map(function (match) {
                        return match[1];
                    });
            });
    }

    function tryUninstall(device) {
        var args = ['-s', device, 'shell', 'pm', 'uninstall'];
        if (!opts.clearstorage) {
            args.push('-k');
        }
        args.push(packageName);

        return spawnWithLogger(api, 'adb', args, {})
            .catch (function () {
                // ignore uninstall errors
            });
    }

    function tryInstall(device) {
        return spawnWithLogger(api, 'adb', ['-s', device, 'install', '-r', apkPath])
            .catch (function () {
                // ignore install errors
            });
    }

    function tryOpen(device) {
        var startCmd = packageName + '/' + packageName + '.' + activityName;
        return spawnWithLogger(api, 'adb', ['-s', device, 'shell', 'am', 'start', '-n', startCmd], {})
            .catch (function () {
                // ignore open errors
            });
    }

    return Promise.try(function () {
        return getDevices();
    })
        .tap(function (devices) {
            if (!devices.length) {
                logger.error('tried to install to device, but no devices found');
            }
        })
        .map(function(device) {
            return tryUninstall(device)
                .then(function () {
                    return tryInstall(device);
                })
                .then(function () {
                    if (opts.open) {
                        return tryOpen(device);
                    }
                });
        });
}
