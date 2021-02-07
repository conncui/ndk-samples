/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include <jni.h>
#include <android/log.h>
#include <assert.h>

#include <unistd.h>
#include <syscall.h>


// Android log function wrappers
static const char* kTAG = "hello-jniCallback";
#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))
#define LOGE2(tag,...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, tag, __VA_ARGS__))



// processing callback to handler class
typedef struct tick_context {
    JavaVM  *javaVM;
    jclass   jniHelperClz;
    jobject  jniHelperObj;
    jclass   mainActivityClz;
    jobject  mainActivityObj;
    pthread_mutex_t  lock;
    int      done;
} TickContext;
TickContext g_ctx;
pthread_mutex_t  lock;


/* This is a trivial JNI example where we use a native method
 * to return a new VM String. See the corresponding Java source
 * file located at:
 *
 *   hello-jniCallback/app/src/main/java/com/example/hellojnicallback/MainActivity.java
 */
extern "C" {
JNIEXPORT jstring JNICALL
Java_com_example_hellojnicallback_MainActivity_stringFromJNI( JNIEnv* env, jobject thiz )
{
#if defined(__arm__)
    #if defined(__ARM_ARCH_7A__)
    #if defined(__ARM_NEON__)
      #if defined(__ARM_PCS_VFP)
        #define ABI "armeabi-v7a/NEON (hard-float)"
      #else
        #define ABI "armeabi-v7a/NEON"
      #endif
    #else
      #if defined(__ARM_PCS_VFP)
        #define ABI "armeabi-v7a (hard-float)"
      #else
        #define ABI "armeabi-v7a"
      #endif
    #endif
  #else
   #define ABI "armeabi"
  #endif
#elif defined(__i386__)
#define ABI "x86"
#elif defined(__x86_64__)
#define ABI "x86_64"
#elif defined(__mips64)  /* mips64el-* toolchain defines __mips__ too */
#define ABI "mips64"
#elif defined(__mips__)
#define ABI "mips"
#elif defined(__aarch64__)
#define ABI "arm64-v8a"
#else
#define ABI "unknown"
#endif
    return env->NewStringUTF( "Hello from JNI !  Compiled with ABI " ABI ".");
}

/*
 *  A helper function to show how to call
 *     java static functions JniHelper::getBuildVersion()
 *     java non-static function JniHelper::getRuntimeMemorySize()
 *  The trivial implementation for these functions are inside file
 *     JniHelper.java
 */
void queryRuntimeInfo(JNIEnv *env, jobject instance) {
    // Find out which OS we are running on. It does not matter for this app
    // just to demo how to call static functions.
    // Our java JniHelper class id and instance are initialized when this
    // shared lib got loaded, we just directly use them
    //    static function does not need instance, so we just need to feed
    //    class and method id to JNI
    jmethodID versionFunc = env->GetStaticMethodID( g_ctx.jniHelperClz,
            "getBuildVersion", "()Ljava/lang/String;");
    if (!versionFunc) {
        LOGE("Failed to retrieve getBuildVersion() methodID @ line %d",
             __LINE__);
        return;
    }
    jstring buildVersion = (jstring)env->CallStaticObjectMethod(
                                                          g_ctx.jniHelperClz, versionFunc);
    const char *version = env->GetStringUTFChars( buildVersion, NULL);
    if (!version) {
        LOGE("Unable to get version string @ line %d", __LINE__);
        return;
    }
    LOGI("Android Version - %s", version);
    LOGE2("tag","Android Version - %s", version);
    env->ReleaseStringUTFChars( buildVersion, version);

    // we are called from JNI_OnLoad, so got to release LocalRef to avoid leaking
    env->DeleteLocalRef( buildVersion);

    // Query available memory size from a non-static public function
    // we need use an instance of JniHelper class to call JNI
    jmethodID memFunc = env->GetMethodID( g_ctx.jniHelperClz,
                                            "getRuntimeMemorySize", "()J");
    if (!memFunc) {
        LOGE("Failed to retrieve getRuntimeMemorySize() methodID @ line %d",
             __LINE__);
        return;
    }
    jlong result = env->CallLongMethod( instance, memFunc);
    LOGI("Runtime free memory size: %" PRId64, result);
    (void)result;  // silence the compiler warning
}

/*
 * processing one time initialization:
 *     Cache the javaVM into our context
 *     Find class ID for JniHelper
 *     Create an instance of JniHelper
 *     Make global reference since we are using them from a native thread
 * Note:
 *     All resources allocated here are never released by application
 *     we rely on system to free all global refs when it goes away;
 *     the pairing function JNI_OnUnload() never gets called at all.
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    memset(&g_ctx, 0, sizeof(g_ctx));

    g_ctx.javaVM = vm;
    if (vm->GetEnv( (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR; // JNI version not supported.
    }

    jclass  clz = env->FindClass(
                                    "com/example/hellojnicallback/JniHandler");
    g_ctx.jniHelperClz = (jclass)env->NewGlobalRef( clz);

    jmethodID  jniHelperCtor = env->GetMethodID( g_ctx.jniHelperClz,
                                                   "<init>", "()V");
    jobject    handler = env->NewObject( g_ctx.jniHelperClz,
                                           jniHelperCtor);
    g_ctx.jniHelperObj = env->NewGlobalRef( handler);
    queryRuntimeInfo(env, g_ctx.jniHelperObj);

    g_ctx.done = 0;
    g_ctx.mainActivityObj = NULL;
    return  JNI_VERSION_1_6;
}

/*
 * A helper function to wrap java JniHelper::updateStatus(String msg)
 * JNI allow us to call this function via an instance even it is
 * private function.
 */
void   sendJavaMsg(JNIEnv *env, jobject instance,
                   jmethodID func,const char* msg) {
    jstring javaMsg = env->NewStringUTF( msg);
    env->CallVoidMethod( instance, func, javaMsg);
    env->DeleteLocalRef( javaMsg);
}

/*
 * Main working thread function. From a pthread,
 *     calling back to MainActivity::updateTimer() to display ticks on UI
 *     calling back to JniHelper::updateStatus(String msg) for msg
 */
void*  UpdateTicks(void* context) {
    TickContext *pctx = (TickContext*) context;
    JavaVM *javaVM = pctx->javaVM;
    JNIEnv *env;
    jint res = javaVM->GetEnv( (void**)&env, JNI_VERSION_1_6);
    if (res != JNI_OK) {
        res = javaVM->AttachCurrentThread( &env, NULL);
        if (JNI_OK != res) {
            LOGE("Failed to AttachCurrentThread, ErrorCode = %d", res);
            return NULL;
        }
    }

    jmethodID statusId = env->GetMethodID( pctx->jniHelperClz,
                                             "updateStatus",
                                             "(Ljava/lang/String;)V");
    sendJavaMsg(env, pctx->jniHelperObj, statusId,
                "TickerThread status: initializing...");

    // get mainActivity updateTimer function
    jmethodID timerId = env->GetMethodID( pctx->mainActivityClz,
                                            "updateTimer", "()V");

    // get mainAc
    // tivity updateTimer function
    jmethodID method_para_long_return_long_id     = env->GetMethodID( pctx->mainActivityClz,"method_para_long_return_long", "(Ljava/lang/Long;)Ljava/lang/Long;");
    jmethodID method_para_long_return_long_id_j     = env->GetMethodID( pctx->mainActivityClz,"method_para_long_return_long", "(Ljava/lang/Long;)Ljava/lang/Long;");
    jmethodID method_para_long_return_obj_id     = env->GetMethodID( pctx->mainActivityClz,"method_para_long_return_obj", "(Ljava/lang/Long;)Ljava/lang/Object;");
//    jmethodID method_para_void_return_obj_id     = env->GetMethodID( pctx->mainActivityClz,"method_para_void_return_obj", "()Ljava/lang/Object;");
    jmethodID method_para_long_return_void_id     = env->GetMethodID( pctx->mainActivityClz,"method_para_long_return_void", "(J)V");

    jmethodID method_para_void_return_void_id     = env->GetMethodID( pctx->mainActivityClz,"method_para_void_return_void", "()V");
    jmethodID method_para_void_return_String_id   = env->GetMethodID( pctx->mainActivityClz,"method_para_void_return_String", "()Ljava/lang/String;");
    jmethodID method_para_string_return_void_id   = env->GetMethodID( pctx->mainActivityClz,"method_para_string_return_void", "(Ljava/lang/String;)V");
    jmethodID method_para_string_return_string_id = env->GetMethodID( pctx->mainActivityClz,"method_para_string_return_string", "(Ljava/lang/String;)Ljava/lang/String;");

    LOGE("jnicallback CallVoidMethod  method_para_long_return_long_id is : %d",method_para_long_return_long_id);
    LOGE("jnicallback CallVoidMethod  method_para_long_return_long_id_j is : %d",method_para_long_return_long_id_j);
    LOGE("jnicallback CallVoidMethod  method_para_long_return_obj is : %d",method_para_long_return_obj_id);
//    LOGE("jnicallback CallVoidMethod  method_para_void_return_obj is : %d",method_para_void_return_obj_id);
    LOGE("jnicallback CallVoidMethod  method_para_long_return_void is : %d",method_para_long_return_void_id);

    LOGE("jnicallback CallVoidMethod  method_para_void_return_void_id is : %d",method_para_void_return_void_id);
    LOGE("jnicallback CallVoidMethod  method_para_void_return_String_id is : %d",method_para_void_return_String_id);
    LOGE("jnicallback CallVoidMethod  method_para_string_return_void_id is : %d",method_para_string_return_void_id);
    LOGE("jnicallback CallVoidMethod  method_para_string_return_string_id is : %d",method_para_string_return_string_id);


    struct timeval beginTime, curTime, usedTime, leftTime;
    const struct timeval kOneSecond = {
            (__kernel_time_t)1,
            (__kernel_suseconds_t) 0
    };

    sendJavaMsg(env, pctx->jniHelperObj, statusId,
                "TickerThread status: start ticking ...");
    while(1) {
        gettimeofday(&beginTime, NULL);
        pthread_mutex_lock(&pctx->lock);
        int done = pctx->done;
        if (pctx->done) {
            pctx->done = 0;
        }
        pthread_mutex_unlock(&pctx->lock);
        if (done) {
            break;
        }
        LOGE("jnicallback CallVoidMethod  timerId is : %d",curTime);


        env->CallVoidMethod( pctx->mainActivityObj, timerId);
        env->CallVoidMethod( pctx->mainActivityObj, method_para_void_return_void_id);

        jstring para_string = env->NewStringUTF( "111");
        jstring return_result__string = (jstring)env->CallObjectMethod( pctx->mainActivityObj, method_para_string_return_string_id,para_string);
        LOGE("jnicallback return_result__string is : %d",return_result__string);
        LOGE("jnicallback return_result__string is : %s",env->GetStringUTFChars(return_result__string,0));

//        env->CallObjectMethodV( pctx->mainActivityObj, method_para_string_return_string_id,"111");
//        env->CallObjectMethodA( pctx->mainActivityObj, method_para_string_return_string_id,"111");
//        env->CallVoidMethod( pctx->mainActivityObj, method_para_string_return_void_id,"");


        gettimeofday(&curTime, NULL);
        timersub(&curTime, &beginTime, &usedTime);
        timersub(&kOneSecond, &usedTime, &leftTime);
        struct timespec sleepTime;
        sleepTime.tv_sec = leftTime.tv_sec;
        sleepTime.tv_nsec = leftTime.tv_usec * 1000;


        LOGE("jnicallback sleepTime.tv_sec is : %d",sleepTime.tv_sec);
        LOGE("jnicallback curTime : %d",curTime);
        if (sleepTime.tv_sec <= 1) {
            nanosleep(&sleepTime, NULL);
        } else {
            sendJavaMsg( env,pctx->jniHelperObj, statusId,
                        "TickerThread error: processing too long!");
        }
    }

    sendJavaMsg(env, pctx->jniHelperObj, statusId,
                "TickerThread status: ticking stopped");
    javaVM->DetachCurrentThread();
    return context;
}

/*
 * Interface to Java side to start ticks, caller is from onResume()
 */

JNIEXPORT void JNICALL
Java_com_example_hellojnicallback_MainActivity_startTicks(JNIEnv *env, jobject instance,
                                                          jstring para_1) {
    pthread_t       threadInfo_;
    pthread_attr_t  threadAttr_;

    pthread_attr_init(&threadAttr_);
    pthread_attr_setdetachstate(&threadAttr_, PTHREAD_CREATE_DETACHED);

    pthread_mutex_init(&g_ctx.lock, NULL);

    jclass clz = env->GetObjectClass( instance);
    g_ctx.mainActivityClz = (jclass)env->NewGlobalRef( clz);
    g_ctx.mainActivityObj = env->NewGlobalRef( instance);

    int result  = pthread_create( &threadInfo_, &threadAttr_, UpdateTicks, &g_ctx);
    assert(result == 0);

    pthread_attr_destroy(&threadAttr_);

    (void)result;
}

/*
 * Interface to Java side to stop ticks:
 *    we need to hold and make sure our native thread has finished before return
 *    for a clean shutdown. The caller is from onPause
 */
extern "C" JNIEXPORT void JNICALL
Java_com_example_hellojnicallback_MainActivity_StopTicks(JNIEnv *env, jobject instance) {
    pthread_mutex_lock(&g_ctx.lock);
    g_ctx.done = 1;
    pthread_mutex_unlock(&g_ctx.lock);

    // waiting for ticking thread to flip the done flag
    struct timespec sleepTime;
    memset(&sleepTime, 0, sizeof(sleepTime));
    sleepTime.tv_nsec = 100000000;
    while (g_ctx.done) {
        nanosleep(&sleepTime, NULL);
    }

    // release object we allocated from StartTicks() function
    env->DeleteGlobalRef( g_ctx.mainActivityClz);
    env->DeleteGlobalRef( g_ctx.mainActivityObj);
    g_ctx.mainActivityObj = NULL;
    g_ctx.mainActivityClz = NULL;

    pthread_mutex_destroy(&g_ctx.lock);
}


void*  UpdateTicks2(void * ctx) {
    int tid = (int)syscall(SYS_gettid);
    int pid = (int)syscall(SYS_getpid);
    LOGE("thread_native UpdateTicks thread tid is %d , pid is %d ",tid,pid);
    return nullptr;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_hellojnicallback_MainActivity_createThread(JNIEnv *env, jobject thiz,
                                                            jstring para1) {
    pthread_t       threadInfo_;
    pthread_attr_t  threadAttr_;

    pthread_attr_init(&threadAttr_);
    pthread_attr_setdetachstate(&threadAttr_, PTHREAD_CREATE_DETACHED);

    pthread_mutex_init(&lock, NULL);


    int tid = (int)syscall(SYS_gettid);
    int pid = (int)syscall(SYS_getpid);
    LOGE("thread_native MainActivity_createThread thread tid is %d , pid is %d ",tid,pid);


    int result  = pthread_create( &threadInfo_, &threadAttr_, UpdateTicks2, NULL);
    assert(result == 0);

    pthread_attr_destroy(&threadAttr_);

    (void)result;
}
}