/*
 * Copyright (C) 2012 Google Inc.
 */
#define LOG_TAG "BluetoothA2dpServiceJni"

#define LOG_NDEBUG 0

#include "com_android_bluetooth.h"
#include "hardware/bt_av.h"
#include "utils/Log.h"
#include "android_runtime/AndroidRuntime.h"

#include <string.h>

namespace android {
static jmethodID method_onConnectionStateChanged;

static const btav_interface_t *sBluetoothA2dpInterface = NULL;
static jobject mCallbacksObj = NULL;
static JNIEnv *sCallbackEnv = NULL;

static bool checkCallbackThread() {
    if (sCallbackEnv == NULL) {
        sCallbackEnv = getCallbackEnv();
    }

    JNIEnv* env = AndroidRuntime::getJNIEnv();
    if (sCallbackEnv != env || sCallbackEnv == NULL) return false;
    return true;
}

static void bta2dp_connection_state_callback(btav_connection_state_t state, bt_bdaddr_t* bd_addr) {
    jbyteArray addr;

    LOGI("%s", __FUNCTION__);

    if (!checkCallbackThread()) {                                       \
        LOGE("Callback: '%s' is not called on the correct thread", __FUNCTION__); \
        return;                                                         \
    }
    addr = sCallbackEnv->NewByteArray(sizeof(bt_bdaddr_t));
    if (!addr) {
        LOGE("Fail to new jbyteArray bd addr for connection state");
        checkAndClearExceptionFromCallback(sCallbackEnv, __FUNCTION__);
        return;
    }

    sCallbackEnv->SetByteArrayRegion(addr, 0, sizeof(bt_bdaddr_t), (jbyte*) bd_addr);
    sCallbackEnv->CallVoidMethod(mCallbacksObj, method_onConnectionStateChanged, (jint) state,
                                 addr);
    checkAndClearExceptionFromCallback(sCallbackEnv, __FUNCTION__);
    sCallbackEnv->DeleteLocalRef(addr);
}

static btav_callbacks_t sBluetoothA2dpCallbacks = {
    sizeof(sBluetoothA2dpCallbacks),
    bta2dp_connection_state_callback,
};

static void classInitNative(JNIEnv* env, jclass clazz) {
    int err;
    const bt_interface_t* btInf;
    bt_status_t status;

    method_onConnectionStateChanged =
        env->GetMethodID(clazz, "onConnectionStateChanged", "(I[B)V");

    if ( (btInf = getBluetoothInterface()) == NULL) {
        LOGE("Bluetooth module is not loaded");
        return;
    }

    if ( (sBluetoothA2dpInterface = (btav_interface_t *)
          btInf->get_profile_interface(BT_PROFILE_ADVANCED_AUDIO_ID)) == NULL) {
        LOGE("Failed to get Bluetooth A2DP Interface");
        return;
    }

    // TODO(BT) do this only once or
    //          Do we need to do this every time the BT reenables?
    if ( (status = sBluetoothA2dpInterface->init(&sBluetoothA2dpCallbacks)) != BT_STATUS_SUCCESS) {
        LOGE("Failed to initialize Bluetooth A2DP, status: %d", status);
        sBluetoothA2dpInterface = NULL;
        return;
    }

    LOGI("%s: succeeds", __FUNCTION__);
}

static void initializeNativeDataNative(JNIEnv *env, jobject object) {
    // TODO(BT) clean it up when a2dp service is stopped
    //          Is there a need to do cleanup since A2DP is always present for phone and tablet?
    mCallbacksObj = env->NewGlobalRef(object);
}

static jboolean connectA2dpNative(JNIEnv *env, jobject object, jbyteArray address) {
    jbyte *addr;
    bt_bdaddr_t * btAddr;
    bt_status_t status;

    LOGI("%s: sBluetoothA2dpInterface: %p", __FUNCTION__, sBluetoothA2dpInterface);
    if (!sBluetoothA2dpInterface) return JNI_FALSE;

    addr = env->GetByteArrayElements(address, NULL);
    btAddr = (bt_bdaddr_t *) addr;
    if (!addr) {
        jniThrowIOException(env, EINVAL);
        return JNI_FALSE;
    }

    if ((status = sBluetoothA2dpInterface->connect((bt_bdaddr_t *)addr)) != BT_STATUS_SUCCESS) {
        LOGE("Failed HF connection, status: %d", status);
    }
    env->ReleaseByteArrayElements(address, addr, 0);
    return (status == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static jboolean disconnectA2dpNative(JNIEnv *env, jobject object, jbyteArray address) {
    jbyte *addr;
    bt_status_t status;

    if (!sBluetoothA2dpInterface) return JNI_FALSE;

    addr = env->GetByteArrayElements(address, NULL);
    if (!addr) {
        jniThrowIOException(env, EINVAL);
        return JNI_FALSE;
    }

    if ( (status = sBluetoothA2dpInterface->disconnect((bt_bdaddr_t *)addr)) != BT_STATUS_SUCCESS) {
        LOGE("Failed HF disconnection, status: %d", status);
    }
    env->ReleaseByteArrayElements(address, addr, 0);
    return (status == BT_STATUS_SUCCESS) ? JNI_TRUE : JNI_FALSE;
}

static JNINativeMethod sMethods[] = {
    {"classInitNative", "()V", (void *) classInitNative},
    {"initializeNativeDataNative", "()V", (void *) initializeNativeDataNative},
    {"connectA2dpNative", "([B)Z", (void *) connectA2dpNative},
    {"disconnectA2dpNative", "([B)Z", (void *) disconnectA2dpNative},
    // TODO(BT) clean up
};

int register_com_android_bluetooth_a2dp(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, "com/android/bluetooth/a2dp/A2dpStateMachine", 
                                    sMethods, NELEM(sMethods));
}

}