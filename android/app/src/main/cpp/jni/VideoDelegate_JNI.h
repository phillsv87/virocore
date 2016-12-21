//
//  VideoDelegate_JNI.h
//  ViroRenderer
//
//  Copyright © 2016 Viro Media. All rights reserved.
//

#ifndef ANDROID_VIDEO_DELEGATE_JNI_H
#define ANDROID_VIDEO_DELEGATE_JNI_H

#include <jni.h>
#include <PersistentRef.h>
#include "VROVideoDelegateInternal.h"

class VideoDelegate : public VROVideoDelegateInternal {
    public:
    VideoDelegate(jobject videoJavaObject, JNIEnv *env);
    ~VideoDelegate();

    static jlong jptr(std::shared_ptr<VideoDelegate> shared_node) {
        PersistentRef<VideoDelegate> *native_surface = new PersistentRef<VideoDelegate>(shared_node);
        return reinterpret_cast<intptr_t>(native_surface);
    }

    static std::shared_ptr<VideoDelegate> native(jlong ptr) {
        PersistentRef<VideoDelegate> *persistentSurface = reinterpret_cast<PersistentRef<VideoDelegate> *>(ptr);
        return persistentSurface->get();
    }

    /**
     * Video Delegate Callbacks
     */
    virtual void videoDidFinish();

    private:
        jobject _javaObject;
        JNIEnv *_env;
};
#endif