//
//  VRORendererARCore_JNI.cpp
//  ViroRenderer
//
//  Created by Raj Advani on 2/24/18.
//  Copyright © 2018 Viro Media. All rights reserved.
//

#include <jni.h>
#include <memory>
#include "VRORenderer_JNI.h"
#include <PersistentRef.h>
#include "VROSceneRendererARCore.h"
#include "ARUtils_JNI.h"
#include "VRORenderer.h"

#if VRO_PLATFORM_ANDROID
#define VRO_METHOD(return_type, method_name) \
  JNIEXPORT return_type JNICALL              \
      Java_com_viro_core_RendererARCore_##method_name
#endif

extern "C" {

VRO_METHOD(VRO_REF, nativeCreateRendererARCore)(VRO_ARGS
                                                jobject class_loader,
                                                jobject android_context,
                                                jobject asset_mgr,
                                                jobject platform_util,
                                                jboolean enableShadows,
                                                jboolean enableHDR,
                                                jboolean enablePBR,
                                                jboolean enableBloom) {
    VROPlatformSetType(VROPlatformType::AndroidARCore);

    std::shared_ptr<gvr::AudioApi> gvrAudio = std::make_shared<gvr::AudioApi>();
    gvrAudio->Init(env, android_context, class_loader, GVR_AUDIO_RENDERING_BINAURAL_HIGH_QUALITY);
    VROPlatformSetEnv(env, android_context, asset_mgr, platform_util);

    VRORendererConfiguration config;
    config.enableShadows = enableShadows;
    config.enableHDR = enableHDR;
    config.enablePBR = enablePBR;
    config.enableBloom = enableBloom;

    std::shared_ptr<VROSceneRenderer> renderer
            = std::make_shared<VROSceneRendererARCore>(config, gvrAudio);
    return Renderer::jptr(renderer);
}

VRO_METHOD(VRO_INT, nativeGetCameraTextureId)(VRO_ARGS
                                              VRO_REF renderer_j) {
    std::shared_ptr<VROSceneRenderer> renderer = Renderer::native(renderer_j);
    std::shared_ptr<VROSceneRendererARCore> arRenderer = std::dynamic_pointer_cast<VROSceneRendererARCore>(renderer);
    return arRenderer->getCameraTextureId();
}

VRO_METHOD(void, nativeSetARCoreSession)(VRO_ARGS
                                         VRO_REF renderer_j,
                                         VRO_REF session_j) {
    std::shared_ptr<VROSceneRenderer> renderer = Renderer::native(renderer_j);
    std::shared_ptr<VROSceneRendererARCore> arRenderer = std::dynamic_pointer_cast<VROSceneRendererARCore>(renderer);
    arRenderer->setARCoreSession(reinterpret_cast<arcore::Session *>(session_j));
}

VRO_METHOD(void, nativeSetARDisplayGeometry)(VRO_ARGS
                                             VRO_REF renderer_j,
                                             VRO_INT rotation, VRO_INT width, VRO_INT height) {
    std::shared_ptr<VROSceneRenderer> renderer = Renderer::native(renderer_j);
    std::shared_ptr<VROSceneRendererARCore> arRenderer = std::dynamic_pointer_cast<VROSceneRendererARCore>(renderer);
    arRenderer->setDisplayGeometry(rotation, width, height);
}

VRO_METHOD(void, nativeSetPlaneFindingMode)(VRO_ARGS
                                            VRO_REF renderer_j,
                                            jboolean enabled) {
    std::shared_ptr<VROSceneRenderer> renderer = Renderer::native(renderer_j);
    std::shared_ptr<VROSceneRendererARCore> arRenderer = std::dynamic_pointer_cast<VROSceneRendererARCore>(renderer);
    arRenderer->setPlaneFindingMode(enabled);
}

void invokeARResultsCallback(std::vector<VROARHitTestResult> &results, jweak weakCallback) {
    JNIEnv *env = VROPlatformGetJNIEnv();
    jclass arHitTestResultClass = env->FindClass("com/viro/core/ARHitTestResult");

    jobjectArray resultsArray = env->NewObjectArray(results.size(), arHitTestResultClass, NULL);
    for (int i = 0; i < results.size(); i++) {
        jobject result = ARUtilsCreateARHitTestResult(results[i]);
        env->SetObjectArrayElement(resultsArray, i, result);
    }

    jobject globalArrayRef = env->NewGlobalRef(resultsArray);
    VROPlatformDispatchAsyncApplication([weakCallback, globalArrayRef] {
        JNIEnv *env = VROPlatformGetJNIEnv();
        jobject callback = env->NewLocalRef(weakCallback);
        VROPlatformCallJavaFunction(callback, "onHitTestFinished",
                                    "([Lcom/viro/core/ARHitTestResult;)V",
                                    globalArrayRef);
        env->DeleteGlobalRef(globalArrayRef);
        env->DeleteWeakGlobalRef(weakCallback);
    });
}

void invokeEmptyARResultsCallback(jweak weakCallback) {
    VROPlatformDispatchAsyncApplication([weakCallback] {
        JNIEnv *env = VROPlatformGetJNIEnv();
        jobject callback = env->NewLocalRef(weakCallback);
        jclass arHitTestResultClass = env->FindClass("com/viro/core/ARHitTestResult");
        jobjectArray emptyArray = env->NewObjectArray(0, arHitTestResultClass, NULL);
        VROPlatformCallJavaFunction(callback, "onHitTestFinished",
                                    "([Lcom/viro/core/ARHitTestResult;)V", emptyArray);
        env->DeleteWeakGlobalRef(weakCallback);
    });
}

void performARHitTest(VROVector3f rayVec, std::weak_ptr<VROSceneRendererARCore> arRenderer_w,
                      jweak weakCallback) {
    std::shared_ptr<VROSceneRendererARCore> arRenderer = arRenderer_w.lock();
    if (!arRenderer) {
        invokeEmptyARResultsCallback(weakCallback);
    }
    else {
        std::vector<VROARHitTestResult> results = arRenderer->performARHitTest(rayVec);
        invokeARResultsCallback(results, weakCallback);
    }
}

void performARHitTestPoint(JNIEnv *env, float x, float y, std::weak_ptr<VROSceneRendererARCore> arRenderer_w,
                           jweak weakCallback) {
    std::shared_ptr<VROSceneRendererARCore> arRenderer = arRenderer_w.lock();
    if (!arRenderer) {
        invokeEmptyARResultsCallback(weakCallback);
    }
    else {
        std::vector<VROARHitTestResult> results = arRenderer->performARHitTest(x, y);
        invokeARResultsCallback(results, weakCallback);
    }
}

VRO_METHOD(void, nativePerformARHitTestWithRay) (VRO_ARGS
                                                 VRO_REF native_renderer,
                                                 VRO_FLOAT_ARRAY ray,
                                                 jobject callback) {
    // Grab ray to perform the AR hit test
    VRO_FLOAT *rayStart = VRO_FLOAT_ARRAY_GET_ELEMENTS(ray);
    VROVector3f rayVec = VROVector3f(rayStart[0], rayStart[1], rayStart[2]);
    VRO_FLOAT_ARRAY_RELEASE_ELEMENTS(ray, rayStart);

    // Create weak pointers for dispatching
    std::shared_ptr<VROSceneRenderer> renderer = Renderer::native(native_renderer);
    std::weak_ptr<VROSceneRendererARCore> arRenderer_w = std::dynamic_pointer_cast<VROSceneRendererARCore>(renderer);
    jweak weakCallback = env->NewWeakGlobalRef(callback);

    VROPlatformDispatchAsyncRenderer([arRenderer_w, weakCallback, rayVec] {
        performARHitTest(rayVec, arRenderer_w, weakCallback);
    });
}

VRO_METHOD(void, nativePerformARHitTestWithPosition) (VRO_ARGS
                                                      VRO_REF native_renderer,
                                                      VRO_FLOAT_ARRAY position,
                                                      jobject callback) {
    std::shared_ptr<VROSceneRenderer> renderer = Renderer::native(native_renderer);

    // Calculate ray to perform the AR hit test
    VRO_FLOAT *positionStart = VRO_FLOAT_ARRAY_GET_ELEMENTS(position);
    VROVector3f positionVec = VROVector3f(positionStart[0], positionStart[1], positionStart[2]);
    VRO_FLOAT_ARRAY_RELEASE_ELEMENTS(position, positionStart);

    VROVector3f cameraVec = renderer->getRenderer()->getCamera().getPosition();
    // the ray we want to use is (given position - camera position)
    VROVector3f rayVec = positionVec - cameraVec;

    // Create weak pointers for dispatching
    std::weak_ptr<VROSceneRendererARCore> arRenderer_w = std::dynamic_pointer_cast<VROSceneRendererARCore>(renderer);
    jweak weakCallback = env->NewWeakGlobalRef(callback);

    VROPlatformDispatchAsyncRenderer([arRenderer_w, weakCallback, rayVec] {
        performARHitTest(rayVec, arRenderer_w, weakCallback);
    });
}

VRO_METHOD(void, nativePerformARHitTestWithPoint) (VRO_ARGS
                                                   VRO_REF native_renderer,
                                                   VRO_FLOAT x, VRO_FLOAT y,
                                                   jobject callback) {
    std::shared_ptr<VROSceneRenderer> renderer = Renderer::native(native_renderer);
    std::weak_ptr<VROSceneRendererARCore> arRenderer_w = std::dynamic_pointer_cast<VROSceneRendererARCore>(renderer);
    jweak weakCallback = env->NewWeakGlobalRef(callback);

    VROPlatformDispatchAsyncRenderer([env, arRenderer_w, weakCallback, x, y] {
        performARHitTestPoint(env, x, y, arRenderer_w, weakCallback);
    });
}

VRO_METHOD(void, nativeEnableTracking) (VRO_ARGS
                                        VRO_REF nativeRenderer,
                                        jboolean shouldTrack) {
    std::shared_ptr<VROSceneRenderer> renderer = Renderer::native(nativeRenderer);
    std::weak_ptr<VROSceneRendererARCore> arRenderer_w = std::dynamic_pointer_cast<VROSceneRendererARCore>(renderer);

    VROPlatformDispatchAsyncRenderer([arRenderer_w, shouldTrack]{
        std::shared_ptr<VROSceneRendererARCore> arRenderer = arRenderer_w.lock();
        if (arRenderer) {
            arRenderer->enableTracking(shouldTrack);
        }
    });
}

}


