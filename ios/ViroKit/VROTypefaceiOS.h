//
//  VROTypefaceiOS.hpp
//  ViroRenderer
//
//  Created by Raj Advani on 11/24/16.
//  Copyright © 2016 Viro Media. All rights reserved.
//

#ifndef VROTypefaceiOS_h
#define VROTypefaceiOS_h

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#include "VROTypeface.h"
#include <string>
#include <ft2build.h>
#include FT_FREETYPE_H

class VRODriver;

class VROTypefaceiOS : public VROTypeface {
    
public:
    
    VROTypefaceiOS(std::string name, int size, std::shared_ptr<VRODriver> driver);
    virtual ~VROTypefaceiOS();
    
    float getLineHeight() const;
    std::shared_ptr<VROGlyph> loadGlyph(unsigned long charCode, bool forRendering);

protected:
    
    void loadFace(std::string name, int size);
    
private:

    std::weak_ptr<VRODriver> _driver;
    FT_Library _ft;
    FT_Face _face;
    
    // Font data must not be deallocated until the typeface is destroyed
    NSData *_fontData;
    NSData *getFontData(CGFontRef cgFont);
  
};

#endif /* VROTypefaceiOS_h */
