#include "jni.h"
#include <log.h>
#include "zip.h"
#include <assert.h>
#include <libgen.h>
#include <utime.h>
#include <sys/stat.h>
#include "AssetExtractor.h"
#include "core/core.h"
#include "js/js.h"
#include <thread>
#include <iostream>
#include <chrono>

using namespace tns;

void AssetExtractor::ExtractAssets(JNIEnv* env, jobject obj, jstring apk, jstring input, jstring outputDir, jboolean _forceOverwrite) {
    auto forceOverwrite = JNI_TRUE == _forceOverwrite;
    auto strApk = jstringToString(env, apk);

    auto baseDir = jstringToString(env, outputDir);

    std::string filePrefix("assets/");
    int prefixLen = filePrefix.length();
    filePrefix.append(jstringToString(env, input));
    
 /* std::string cInput = jstringToString(env, input);
    std::string assets_dir_str("file:///android_asset/");
    if(cInput.compare(assets_dir_str) == 0){
    filePrefix = cInput;
    }
    else {
    filePrefix.append(cInput);
    }
    */
    auto prfx = filePrefix.c_str();

    int err = 0;
    auto z = zip_open(strApk.c_str(), ZIP_CREATE, &err);
    assert(z != nullptr);
    zip_int64_t num = zip_get_num_entries(z, 0);
    struct zip_stat sb;
    struct zip_file* zf;
    char buf[65536];
    auto pathcopy = new char[1024];

    for (zip_int64_t i = 0; i < num; i++) {
        zip_stat_index(z, i, ZIP_STAT_MTIME, &sb);
        if (strstr(sb.name, prfx) == sb.name) {
            auto name = sb.name + prefixLen; // strlen("assets/") == 7

            std::string assetFullname(baseDir);
            assetFullname.append(name);

            struct stat attrib;
            auto shouldOverwrite = true;
            int ret = stat(assetFullname.c_str(), &attrib);
            LOG("{js} extract assets %s", assetFullname.c_str());
            if (ret == 0 /* file exists */) {
                auto diff = difftime(sb.mtime, attrib.st_mtime);
                shouldOverwrite = diff > 0;
            }

            if (shouldOverwrite || forceOverwrite) {
                strcpy(pathcopy, name);
                auto path = dirname(pathcopy);
                std::string dirFullname(baseDir);
                dirFullname.append(path);
                mkdir_rec(dirFullname.c_str());

                zf = zip_fopen_index(z, i, 0);
                assert(zf != nullptr);

                auto fd = fopen(assetFullname.c_str(), "w");

                if (fd != nullptr) {
                    zip_int64_t sum = 0;
                    while (sum != sb.size) {
                        zip_int64_t len = zip_fread(zf, buf, sizeof(buf));
                        assert(len > 0);

                        fwrite(buf, 1, len, fd);
                        sum += len;
                    }
                    fclose(fd);
                    utimbuf t;
                    t.modtime = sb.mtime;
                    utime(assetFullname.c_str(), &t);
                }
               
                zip_fclose(zf);
            }
        }
    }
  
    
    //1 Разогреть девайс до крашей, Поставить точку останова на line 103 этого документа
    // и во время останова следить за размером файла
    //2 Поискать проблему чтения
    //3 REFRESH FILE DATA FROM OS BEFORE START READING IT
    // checkFileSize(int lastLenght){
      // int currentLenght = strlenght(string);
      // if(currentLenghth > lastLenghth){
          // Thread.sleep(1000ms);
          // checkFileSize(currentLenghth)}
          // else{
              // conitune logics to run script
              //}
    //}
    //4 Вернуть thread sleep 5000 пере runNativeJs
    delete[] pathcopy;
    zip_close(z);
    //std::this_thread::sleep_for (std::chrono::seconds(5));
    core_set_is_extracted(true);
                         setAssetsExtracted();
}

void AssetExtractor::mkdir_rec(const char* dir) {
    char opath[256];
    snprintf(opath, sizeof(opath), "%s", dir);
    size_t len = strlen(opath);

    if (opath[len - 1] == '/') {
        opath[len - 1] = 0;
    }

    for (char* p = opath + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(opath, S_IRWXU);
            *p = '/';
        }
    }

    mkdir(opath, S_IRWXU);
}

std::string AssetExtractor::jstringToString(JNIEnv* env, jstring value) {
    if (value == nullptr) {
        return std::string();
    }

    jboolean f = false;
    const char* chars = env->GetStringUTFChars(value, &f);
    std::string s(chars);
    env->ReleaseStringUTFChars(value, chars);

    return s;
}
