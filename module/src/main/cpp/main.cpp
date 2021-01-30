#include <jni.h>
#include <sys/types.h>
#include <riru.h>
#include <malloc.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "nativehelper/scoped_utf_chars.h"
#include "logging.h"
#include "misc.h"

static bool sHookEnable = false;
static char *sAppDataDir = NULL;

static char *jstring2char(JNIEnv *env, jstring target) {
    char *result = nullptr;
    if (target) {
        const char *targetChar = env->GetStringUTFChars(target, nullptr);
        if (targetChar != nullptr) {
            int len = strlen(targetChar);
            result = (char *) malloc((len + 1) * sizeof(char));
            if (result != nullptr) {
                memset(result, 0, len + 1);
                memcpy(result, targetChar, len);
            }
            env->ReleaseStringUTFChars(target, targetChar);
        }
    }
    return result;
}

static bool equals(const char *target1, const char *target2) {
    if (target1 == nullptr && target2 == nullptr) {
        return true;
    } else {
        if (target1 != nullptr && target2 != nullptr) {
            return strcmp(target1, target2) == 0;
        } else {
            return false;
        }
    }
}

static void pre(JNIEnv *env, jstring *appDataDir, jstring *niceName) {
    char *cAppDataDir = jstring2char(env, *appDataDir);
    if (cAppDataDir == NULL) {
        LOGD("MEM ERR");
        return;
    }
    sAppDataDir = strdup(cAppDataDir);
    free(cAppDataDir);
    if (sAppDataDir == NULL) {
        LOGD("MEM ERR");
        return;
    }
    char *cNiceName = jstring2char(env, *niceName);
    sHookEnable = equals(cNiceName, "com.android.settings");
    if (cNiceName) {
        free(cNiceName);
    }
}

void inject(JNIEnv *env) {
    if (env == nullptr) {
        LOGW("failed to inject for com.samsung.android.settings due to env is null");
        return;
    }
    LOGI("inject for com.samsung.android.settings ");

    jclass network_controller = env->FindClass("com/samsung/android/settings/notification/StatusBarNetworkSpeedController");

    if (network_controller == nullptr) {
        LOGW("failed to find network speed controller");
        return;
    }

    jfieldID support_network_speed = env->GetStaticFieldID(network_controller, "SUPPORT_NETWORK_SPEED", "Ljava/lang/Boolean;");

    if (support_network_speed != nullptr) {
        env->SetStaticBooleanField(network_controller, support_network_speed, JNI_TRUE);
    }
    else {
        LOGW("failed to find SUPPORT_NETWORK_SPEED");
    }

    if(env->ExceptionCheck())
    {
        env->ExceptionClear();
    }
}

void post(JNIEnv *env)
{
    if (sHookEnable) {
        inject(env);
    }
}

static void forkAndSpecializePre(
        JNIEnv *env, jclass clazz, jint *_uid, jint *gid, jintArray *gids, jint *runtimeFlags,
        jobjectArray *rlimits, jint *mountExternal, jstring *seInfo, jstring *niceName,
        jintArray *fdsToClose, jintArray *fdsToIgnore, jboolean *is_child_zygote,
        jstring *instructionSet, jstring *appDataDir, jboolean *isTopApp, jobjectArray *pkgDataInfoList,
        jobjectArray *whitelistedDataInfoList, jboolean *bindMountAppDataDirs, jboolean *bindMountAppStorageDirs) {
    pre(env, appDataDir, niceName);
}

static void forkAndSpecializePost(JNIEnv *env, jclass clazz, jint res) {
    if (res == 0) {
        // in app process
        post(env);
    } else {
        // in zygote process, res is child pid
        // don't print log here, see https://github.com/RikkaApps/Riru/blob/77adfd6a4a6a81bfd20569c910bc4854f2f84f5e/riru-core/jni/main/jni_native_method.cpp#L55-L66
    }
}

static void specializeAppProcessPre(
        JNIEnv *env, jclass clazz, jint *_uid, jint *gid, jintArray *gids, jint *runtimeFlags,
        jobjectArray *rlimits, jint *mountExternal, jstring *seInfo, jstring *niceName,
        jboolean *startChildZygote, jstring *instructionSet, jstring *appDataDir,
        jboolean *isTopApp, jobjectArray *pkgDataInfoList, jobjectArray *whitelistedDataInfoList,
        jboolean *bindMountAppDataDirs, jboolean *bindMountAppStorageDirs) {
    // added from Android 10, but disabled at least in Google Pixel devices
    pre(env, appDataDir, niceName);
}

static void specializeAppProcessPost(
        JNIEnv *env, jclass clazz) {
    // added from Android 10, but disabled at least in Google Pixel devices
    post(env);
}

static void forkSystemServerPre(
        JNIEnv *env, jclass clazz, uid_t *uid, gid_t *gid, jintArray *gids, jint *runtimeFlags,
        jobjectArray *rlimits, jlong *permittedCapabilities, jlong *effectiveCapabilities) {

}

static void forkSystemServerPost(JNIEnv *env, jclass clazz, jint res) {
    if (res == 0) {
        // in system server process
    } else {
        // in zygote process, res is child pid
        // don't print log here, see https://github.com/RikkaApps/Riru/blob/77adfd6a4a6a81bfd20569c910bc4854f2f84f5e/riru-core/jni/main/jni_native_method.cpp#L55-L66
    }
}

static int shouldSkipUid(int uid) {
    // by default, Riru only call module functions in "normal app processes" (10000 <= uid % 100000 <= 19999)
    // false = don't skip
    return false;
}

static void onModuleLoaded() {
    // called when the shared library of Riru core is loaded
}

extern "C" {

int riru_api_version;
RiruApiV9 *riru_api_v9;

/*
 * Init will be called three times.
 *
 * The first time:
 *   Returns the highest version number supported by both Riru and the module.
 *
 *   arg: (int *) Riru's API version
 *   returns: (int *) the highest possible API version
 *
 * The second time:
 *   Returns the RiruModuleX struct created by the module.
 *   (X is the return of the first call)
 *
 *   arg: (RiruApiVX *) RiruApi strcut, this pointer can be saved for further use
 *   returns: (RiruModuleX *) RiruModule strcut
 *
 * The second time:
 *   Let the module to cleanup (such as RiruModuleX struct created before).
 *
 *   arg: null
 *   returns: (ignored)
 *
 */
void *init(void *arg) {
    static int step = 0;
    step += 1;

    static void *_module;

    switch (step) {
        case 1: {
            auto core_max_api_version = *(int *) arg;
            riru_api_version = core_max_api_version <= RIRU_MODULE_API_VERSION ? core_max_api_version : RIRU_MODULE_API_VERSION;
            return &riru_api_version;
        }
        case 2: {
            switch (riru_api_version) {
                // RiruApiV10 and RiruModuleInfoV10 are equal to V9
                case 10:
                case 9: {
                    riru_api_v9 = (RiruApiV9 *) arg;

                    auto module = (RiruModuleInfoV9 *) malloc(sizeof(RiruModuleInfoV9));
                    memset(module, 0, sizeof(RiruModuleInfoV9));
                    _module = module;

                    module->supportHide = true;

                    module->version = RIRU_MODULE_VERSION;
                    module->versionName = RIRU_MODULE_VERSION_NAME;
                    module->onModuleLoaded = onModuleLoaded;
                    module->shouldSkipUid = shouldSkipUid;
                    module->forkAndSpecializePre = forkAndSpecializePre;
                    module->forkAndSpecializePost = forkAndSpecializePost;
                    module->specializeAppProcessPre = specializeAppProcessPre;
                    module->specializeAppProcessPost = specializeAppProcessPost;
                    module->forkSystemServerPre = forkSystemServerPre;
                    module->forkSystemServerPost = forkSystemServerPost;
                    return module;
                }
                default: {
                    return nullptr;
                }
            }
        }
        case 3: {
            free(_module);
            return nullptr;
        }
        default: {
            return nullptr;
        }
    }
}
}
