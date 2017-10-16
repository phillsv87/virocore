//
//  VROSceneRendererARCore.cpp
//  ViroRenderer
//
//  Created by Raj Advani on 9/27/178.
//  Copyright © 2017 Viro Media. All rights reserved.
//

#include "VROSceneRendererARCore.h"

#include <android/log.h>
#include <assert.h>
#include <stdlib.h>
#include <cmath>
#include <random>
#include <VROTime.h>
#include <VROProjector.h>
#include <VROARHitTestResult.h>
#include <VROInputControllerAR.h>
#include "VROARCamera.h"
#include "arcore/VROARSessionARCore.h"
#include "arcore/VROARFrameARCore.h"
#include "arcore/VROARCameraARCore.h"

#include "VRODriverOpenGLAndroid.h"
#include "VROGVRUtil.h"
#include "VRONodeCamera.h"
#include "VROMatrix4f.h"
#include "VROViewport.h"
#include "VRORenderer.h"
#include "VROSurface.h"
#include "VRONode.h"
#include "VROARScene.h"
#include "VROInputControllerCardboard.h"
#include "VROAllocationTracker.h"
#include "VROInputControllerARAndroid.h"

static VROVector3f const kZeroVector = VROVector3f();

#pragma mark - Setup

VROSceneRendererARCore::VROSceneRendererARCore(std::shared_ptr<gvr::AudioApi> gvrAudio,
                                               jni::Object<arcore::Session> sessionJNI) :
    _rendererSuspended(true),
    _suspendedNotificationTime(VROTimeCurrentSeconds()),
    _hasTrackingInitialized(false),
    _hasTrackingResumed(false) {

    _driver = std::make_shared<VRODriverOpenGLAndroid>(gvrAudio);
    _session = std::make_shared<VROARSessionARCore>(sessionJNI, _driver);

    // instantiate the input controller w/ viewport size (0,0) and update it later.
    std::shared_ptr<VROInputControllerAR> controller = std::make_shared<VROInputControllerARAndroid>(0,0);

    _renderer = std::make_shared<VRORenderer>(controller);
    controller->setRenderer(_renderer);
    controller->setSession(_session);

    _pointOfView = std::make_shared<VRONode>();
    _pointOfView->setCamera(std::make_shared<VRONodeCamera>());
    _renderer->setPointOfView(_pointOfView);
}

VROSceneRendererARCore::~VROSceneRendererARCore() {

}

#pragma mark - Rendering

void VROSceneRendererARCore::initGL() {
    _session->initGL(_driver);
}

void VROSceneRendererARCore::onDrawFrame() {
    if (!_rendererSuspended) {
        renderFrame();
    }
    else {
        renderSuspended();
    }

    ++_frame;
    ALLOCATION_TRACKER_PRINT();
}

void VROSceneRendererARCore::renderFrame() {
    /*
     Setup GL state.
     */
    glEnable(GL_DEPTH_TEST);
    _driver->setCullMode(VROCullMode::Back);

    VROViewport viewport(0, 0, _surfaceSize.width, _surfaceSize.height);

    if (_sceneController) {
        if (!_cameraBackground) {
            initARSession(viewport, _sceneController->getScene());
        }
    }

    if (_session->isReady()) {
        _session->setViewport(viewport);

        /*
         Retrieve transforms from the AR session.
         */
        std::unique_ptr<VROARFrame> &frame = _session->updateFrame();
        const std::shared_ptr<VROARCamera> camera = frame->getCamera();

        /*
         If we attempt to get the projection matrix from the session before tracking has
         resumed (even if the session itself has been resumed) we'll get a SessionPausedException.
         Protect against this by not accessing the session until tracking is operational.
         */
        if (_hasTrackingResumed || camera->getTrackingState() == VROARTrackingState::Normal) {
            renderWithTracking(camera, frame, viewport, !_hasTrackingResumed);
            _hasTrackingResumed = true;
        }
        else {
            renderWaitingForTracking(viewport);
        }
    }
    else {
        renderWaitingForTracking(viewport);
    }
}

void VROSceneRendererARCore::renderWithTracking(const std::shared_ptr<VROARCamera> &camera,
                                                const std::unique_ptr<VROARFrame> &frame,
                                                VROViewport viewport,
                                                bool firstFrameSinceResume) {
    VROFieldOfView fov;
    VROMatrix4f projection = camera->getProjection(viewport, kZNear, _renderer->getFarClippingPlane(), &fov);
    VROMatrix4f rotation = camera->getRotation();
    VROVector3f position = camera->getPosition();

    if (_sceneController && !_hasTrackingInitialized) {
        if (position != kZeroVector) {
            _hasTrackingInitialized = true;

            std::shared_ptr<VROARScene> arScene = std::dynamic_pointer_cast<VROARScene>(_sceneController->getScene());
            passert_msg (arScene != nullptr, "AR View requires an AR Scene!");
            arScene->trackingHasInitialized();
        }
    }

    if (firstFrameSinceResume || ((VROARFrameARCore *)frame.get())->isDisplayRotationChanged()) {
        VROVector3f BL, BR, TL, TR;
        ((VROARFrameARCore *)frame.get())->getBackgroundTexcoords(&BL, &BR, &TL, &TR);

        _cameraBackground->setTextureCoordinates(BL, BR, TL, TR);

        // Wait until we have these proper texture coordinates before installing the background
        if (!_sceneController->getScene()->getRootNode()->getBackground()) {
            _sceneController->getScene()->getRootNode()->setBackground(_cameraBackground);
        }
    }

    /*
     Render the 3D scene.
     */
    _pointOfView->getCamera()->setPosition(position);
    _renderer->prepareFrame(_frame, viewport, fov, rotation, projection, _driver);

    glViewport(viewport.getX(), viewport.getY(), viewport.getWidth(), viewport.getHeight());
    _renderer->renderEye(VROEyeType::Monocular, VROMatrix4f::identity(), projection, viewport, _driver);
    _renderer->endFrame(_driver);

    /*
     Notify scene of the updated ambient light estimates
     */
    std::shared_ptr<VROARScene> scene = std::dynamic_pointer_cast<VROARScene>(_session->getScene());
    scene->updateAmbientLight(frame->getAmbientLightIntensity(), frame->getAmbientLightColorTemperature());
}

void VROSceneRendererARCore::renderWaitingForTracking(VROViewport viewport) {
    /*
     Render black while waiting for the AR session to initialize.
     */
    VROFieldOfView fov = _renderer->computeMonoFOV(viewport.getWidth(), viewport.getHeight());
    VROMatrix4f projection = fov.toPerspectiveProjection(kZNear, _renderer->getFarClippingPlane());

    _renderer->prepareFrame(_frame, viewport, fov, VROMatrix4f::identity(), projection, _driver);
    glViewport(viewport.getX(), viewport.getY(), viewport.getWidth(), viewport.getHeight());
    _renderer->renderEye(VROEyeType::Monocular, VROMatrix4f::identity(), projection, viewport, _driver);
    _renderer->endFrame(_driver);
}

void VROSceneRendererARCore::renderSuspended() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Notify the user about bad keys 5 times a second (every 200ms/.2s)
    double newTime = VROTimeCurrentSeconds();
    if (newTime - _suspendedNotificationTime > .2) {
        perr("Renderer suspended! Do you have a valid key?");
        _suspendedNotificationTime = newTime;
    }
}

void VROSceneRendererARCore::initARSession(VROViewport viewport, std::shared_ptr<VROScene> scene) {
    // Create the background surface
    _cameraBackground = VROSurface::createSurface(viewport.getX() + viewport.getWidth() / 2.0,
                                                  viewport.getY() + viewport.getHeight() / 2.0,
                                                  viewport.getWidth(), viewport.getHeight(),
                                                  0, 0, 1, 1);
    _cameraBackground->setScreenSpace(true);
    _cameraBackground->setName("Camera");

    // Assign the background texture to the background surface
    std::shared_ptr<VROMaterial> material = _cameraBackground->getMaterials()[0];
    material->setLightingModel(VROLightingModel::Constant);
    material->getDiffuse().setTexture(_session->getCameraBackgroundTexture());
    material->setWritesToDepthBuffer(false);

    _componentManager = std::make_shared<VROARComponentManager>();

    _session->setScene(scene);
    _session->setViewport(viewport);
    _session->setAnchorDetection({VROAnchorDetection::PlanesHorizontal});
    _session->setDelegate(_componentManager);

    _session->run();


    std::shared_ptr<VROARScene> arScene = std::dynamic_pointer_cast<VROARScene>(scene);
    passert_msg (arScene != nullptr, "AR View requires an AR Scene!");

    arScene->setARComponentManager(_componentManager);
    arScene->addNode(_pointOfView);
}

/*
 Update render sizes as the surface changes.
 */
void VROSceneRendererARCore::onSurfaceChanged(jobject surface, jint width, jint height) {
    VROThreadRestricted::setThread(VROThreadName::Renderer, pthread_self());

    _surfaceSize.width = width;
    _surfaceSize.height = height;

    if (_cameraBackground) {
        _cameraBackground->setX(width / 2.0);
        _cameraBackground->setY(height / 2.0);
        _cameraBackground->setWidth(width);
        _cameraBackground->setHeight(height);
    }

    std::shared_ptr<VROInputControllerARAndroid> inputControllerAR =
            std::dynamic_pointer_cast<VROInputControllerARAndroid>(_renderer->getInputController());

    if (inputControllerAR) {
        inputControllerAR->setViewportSize(width, height);
    }
}

void VROSceneRendererARCore::onTouchEvent(int action, float x, float y) {
    std::shared_ptr<VROInputControllerBase> baseController = _renderer->getInputController();
    std::shared_ptr<VROInputControllerARAndroid> arTouchController
            = std::dynamic_pointer_cast<VROInputControllerARAndroid>(baseController);
    arTouchController->onTouchEvent(action, x, y);
}

void VROSceneRendererARCore::onPinchEvent(int pinchState, float scaleFactor,
                                          float viewportX, float viewportY) {
    std::shared_ptr<VROInputControllerBase> baseController = _renderer->getInputController();
    std::shared_ptr<VROInputControllerARAndroid> arTouchController
            = std::dynamic_pointer_cast<VROInputControllerARAndroid>(baseController);
    arTouchController->onPinchEvent(pinchState, scaleFactor, viewportX, viewportY);
}
void VROSceneRendererARCore::onRotateEvent(int rotateState, float rotateDegrees, float viewportX,
                                           float viewportY) {
    std::shared_ptr<VROInputControllerBase> baseController = _renderer->getInputController();
    std::shared_ptr<VROInputControllerARAndroid> arTouchController
            = std::dynamic_pointer_cast<VROInputControllerARAndroid>(baseController);
    arTouchController->onRotateEvent(rotateState, rotateDegrees, viewportX, viewportY);
}

void VROSceneRendererARCore::onPause() {
    std::shared_ptr<VROSceneRendererARCore> shared = shared_from_this();
    VROPlatformDispatchAsyncRenderer([shared] {
        shared->_renderer->getInputController()->onPause();
        shared->_driver->onPause();
    });
}

void VROSceneRendererARCore::onResume() {
    std::shared_ptr<VROSceneRendererARCore> shared = shared_from_this();

    VROPlatformDispatchAsyncRenderer([shared] {
        shared->_renderer->getInputController()->onResume();
        shared->_driver->onResume();
    });

    // Place this here instead of onPause(), just in case a stray render occurs
    // after pause (while shutting down the GL thread) that flips it back to
    // true.
    _hasTrackingResumed = false;
}

void VROSceneRendererARCore::setVRModeEnabled(bool enabled) {

}

void VROSceneRendererARCore::setSuspended(bool suspendRenderer) {
    _rendererSuspended = suspendRenderer;
}

void VROSceneRendererARCore::setSceneController(std::shared_ptr<VROSceneController> sceneController) {
    _sceneController = sceneController;
    std::shared_ptr<VROARScene> arScene = std::dynamic_pointer_cast<VROARScene>(sceneController->getScene());
    if (arScene && _componentManager) {
        arScene->setARComponentManager(_componentManager);
    }
    VROSceneRenderer::setSceneController(sceneController);
}

void VROSceneRendererARCore::setSceneController(std::shared_ptr<VROSceneController> sceneController, float seconds,
                        VROTimingFunctionType timingFunction) {

    _sceneController = sceneController;
    VROSceneRenderer::setSceneController(sceneController, seconds, timingFunction);
}

std::vector<VROARHitTestResult> VROSceneRendererARCore::performARHitTest(VROVector3f ray) {
    VROVector3f cameraForward = getRenderer()->getRenderContext()->getCamera().getForward();

    if (cameraForward.dot(ray) <= 0) {
        return std::vector<VROARHitTestResult>();
    }

    int viewportArr[4] = {0, 0, _surfaceSize.width, _surfaceSize.height};

    // create the mvp (in this case, the model mat is identity).
    VROMatrix4f projectionMat = getRenderer()->getRenderContext()->getProjectionMatrix();
    VROMatrix4f viewMat = getRenderer()->getRenderContext()->getViewMatrix();
    VROMatrix4f vpMat = projectionMat.multiply(viewMat);

    VROVector3f point;
    VROProjector::project(ray, vpMat.getArray(), viewportArr, &point);

    std::unique_ptr<VROARFrame> &frame = _session->getLastFrame();
    if (frame && point.x >= 0 && point.x <= viewportArr[2] && point.y >= 0 && point.y <= viewportArr[3]) {
        std::vector<VROARHitTestResult> results = frame->hitTest(point.x,
                                                                 point.y,
                                                                 {VROARHitTestResultType::ExistingPlaneUsingExtent,
                                                                  VROARHitTestResultType::ExistingPlane,
                                                                  VROARHitTestResultType::EstimatedHorizontalPlane,
                                                                  VROARHitTestResultType::FeaturePoint});
        return results;
    };

    return std::vector<VROARHitTestResult>();
}



