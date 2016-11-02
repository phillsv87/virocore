//
//  VROSceneController.m
//  ViroRenderer
//
//  Created by Raj Advani on 3/25/16.
//  Copyright © 2016 Viro Media. All rights reserved.
//

#import "VROSceneController.h"
#import "VROSceneControllerInternal.h"
#import "VROHoverDelegate.h"
#import "VROTransaction.h"
#import "VROScene.h"
#import "VROMaterial.h"
#import "VROGeometry.h"
#import "VRONode.h"
#import "VROReticleSizeListener.h"
#import "VROScreenUIView.h"
#import "VROFrameSynchronizer.h"
#import "VROView.h"
#import <map>

@interface VROSceneController ()

@property (readwrite, nonatomic) std::shared_ptr<VROHoverDelegate> hoverDelegate;
@property (readwrite, nonatomic) std::shared_ptr<VROSceneControllerInternal> internal;

@end

@implementation VROSceneController

- (id)initWithView:(id <VROView>)view {
    self = [super init];
    if (self) {
        std::shared_ptr<VROHoverDistanceListener> listener = std::make_shared<VROReticleSizeListener>(view);
        self.internal = std::make_shared<VROSceneControllerInternal>(listener, [view frameSynchronizer]);
    }
    
    return self;
}

- (void)sceneWillAppear:(VRORenderContext *)context driver:(VRODriver *)driver {
    self.internal->onSceneWillAppear(*context, *driver);
}

- (void)sceneDidAppear:(VRORenderContext *)context driver:(VRODriver *)driver {
    self.internal->onSceneDidAppear(*context, *driver);
}

- (void)sceneWillDisappear:(VRORenderContext *)context driver:(VRODriver *)driver {
    self.internal->onSceneWillDisappear(*context, *driver);
}

- (void)sceneDidDisappear:(VRORenderContext *)context driver:(VRODriver *)driver {
    self.internal->onSceneDidDisappear(*context, *driver);
}

- (void)startIncomingTransition:(VRORenderContext *)context duration:(float)seconds {    
    // Default animation

    float flyInDistance = 25;
    
    if (self.scene->getBackground()) {
        self.scene->getBackground()->getMaterials().front()->setTransparency(0.0);
    }
    
    std::map<std::shared_ptr<VRONode>, VROVector3f> finalPositions;
    for (std::shared_ptr<VRONode> root : self.scene->getRootNodes()) {
        VROVector3f position = root->getPosition();
        finalPositions[root] = position;
        
        root->setPosition({position.x, position.y, position.z + flyInDistance});
    }
    
    VROTransaction::begin();
    VROTransaction::setAnimationDuration(seconds);
    VROTransaction::setTimingFunction(VROTimingFunctionType::EaseIn);
    
    for (std::shared_ptr<VRONode> root : self.scene->getRootNodes()) {
        VROVector3f position = finalPositions[root];
        root->setPosition(position);
    }
    if (self.scene->getBackground()) {
        self.scene->getBackground()->getMaterials().front()->setTransparency(1.0);
    }
    
    VROTransaction::commit();
}

- (void)startOutgoingTransition:(VRORenderContext *)context duration:(float)seconds {
    // Default animation
    float flyOutDistance = 70;
    
    VROTransaction::begin();
    VROTransaction::setAnimationDuration(seconds);
    VROTransaction::setTimingFunction(VROTimingFunctionType::EaseIn);
    
    for (std::shared_ptr<VRONode> root : self.scene->getRootNodes()) {
        VROVector3f position = root->getPosition();
        root->setPosition({position.x, position.y, position.z - flyOutDistance});
    }
    if (self.scene->getBackground()) {
        self.scene->getBackground()->getMaterials().front()->setTransparency(0.0);
    }
    
    VROTransaction::commit();
}

- (void)endIncomingTransition:(VRORenderContext *)context {
    
}

- (void)endOutgoingTransition:(VRORenderContext *)context {
    
}

- (void)animateIncomingTransition:(VRORenderContext *)context percentComplete:(float)t {
    
}

- (void)animateOutgoingTransition:(VRORenderContext *)context percentComplete:(float)t {
    
}

- (void)setHoverDelegate:(std::shared_ptr<VROHoverDelegate>)delegate {
    self.internal->setHoverDelegate(delegate);
    _hoverDelegate = delegate;
}

- (void)sceneWillRender:(const VRORenderContext *)context {
    
}

- (BOOL)isHoverable:(std::shared_ptr<VRONode>)node {
    return NO;
}

- (void)hoverOnNode:(std::shared_ptr<VRONode>)node {
    
}

- (void)hoverOffNode:(std::shared_ptr<VRONode>)node {
    
}

- (void)setHoverEnabled:(BOOL)enabled boundsOnly:(BOOL)boundsOnly {
    __weak VROSceneController *weakSelf = self;
    if (enabled) {
        self.hoverDelegate = std::make_shared<VROHoverDelegate>(boundsOnly,
                                                                [weakSelf](std::shared_ptr<VRONode> node){
                                                                    return [weakSelf isHoverable:node];
                                                                },
                                                                [weakSelf](std::shared_ptr<VRONode> node){
                                                                    [weakSelf hoverOnNode:node];
                                                                },
                                                                [weakSelf](std::shared_ptr<VRONode> node){
                                                                    [weakSelf hoverOffNode:node];
                                                                });
        
    } else {
        self.hoverDelegate = nil;
    }
}

- (void)reticleTapped:(VROVector3f)ray context:(const VRORenderContext *)context {
    
}

- (std::shared_ptr<VROScene>)scene {
    return self.internal->getScene();
}

@end