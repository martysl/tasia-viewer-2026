/**
 * @file llgltexture.cpp
 * @brief Opengl texture implementation
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "llgltexture.h"


LLGLTexture::LLGLTexture(bool usemipmaps)
{
    init();
    mUseMipMaps = usemipmaps;
}

LLGLTexture::LLGLTexture(const U32 width, const U32 height, const U8 components, bool usemipmaps)
{
    init();
    mFullWidth = width ;
    mFullHeight = height ;
    mUseMipMaps = usemipmaps;
    mComponents = components ;
#ifdef DX_RENDER
    mDXTexturep = new LLImageDX(width, height, components, usemipmaps) ;
#endif
    setTexelsPerImage();
}

LLGLTexture::LLGLTexture(const LLImageRaw* raw, bool usemipmaps)
{
    init();
    mUseMipMaps = usemipmaps ;
    // Create an empty image of the specified size and width
    mGLTexturep = new LLImageGL(raw, usemipmaps) ;
#ifdef DX_RENDER
    // Same raw image, same flags - the DX half stays complete rather than being
    // an empty placeholder, so LLTexUnit::bindFast() has a real SRV to bind.
    mDXTexturep = new LLImageDX(raw, usemipmaps) ;
#endif
    mFullWidth = mGLTexturep->getWidth();
    mFullHeight = mGLTexturep->getHeight();
    mComponents = mGLTexturep->getComponents();
    setTexelsPerImage();
}

LLGLTexture::~LLGLTexture()
{
    cleanup();
}

void LLGLTexture::init()
{
    mBoostLevel = LLGLTexture::BOOST_NONE;
    // <FS:minerjr> [FIRE-36016] - Re-added Store/Restore boost levels of selected objects
    // Added a previous boost level to allow for restoring boost after BOOST_SELECTED is applied
    mPrevBoostLevel = LLGLTexture::BOOST_NONE;
    // </FS:minerjr> [FIRE-36016]

    mFullWidth = 0;
    mFullHeight = 0;
    mTexelsPerImage = 0 ;
    mUseMipMaps = false ;
    mComponents = 0 ;

    mTextureState = NO_DELETE ;
    mDontDiscard = false;
    mNeedsGLTexture = false ;
}

void LLGLTexture::cleanup()
{
    if(mGLTexturep)
    {
        mGLTexturep->cleanup();
    }
}

// virtual
void LLGLTexture::dump()
{
    if(mGLTexturep)
    {
        mGLTexturep->dump();
    }
}

void LLGLTexture::setBoostLevel(S32 level)
{
    if(mBoostLevel != level)
    {
        mBoostLevel = level ;
        if(mBoostLevel != LLGLTexture::BOOST_NONE
           && mBoostLevel != LLGLTexture::BOOST_ICON
           && mBoostLevel != LLGLTexture::BOOST_THUMBNAIL
           && mBoostLevel != LLGLTexture::BOOST_TERRAIN)
        {
            setNoDelete() ;
        }
    }
}

// <FS:minerjr> [FIRE-36016] - Re-added Store/Restore boost levels of selected objects
// Changes the current boost level back to the previous value
void LLGLTexture::restoreBoostLevel()
{
    mBoostLevel = mPrevBoostLevel;
}

// Stores the current boost level in the previous boost
void LLGLTexture::storeBoostLevel()
{
    mPrevBoostLevel = mBoostLevel;
}
// </FS:minerjr> [FIRE-36016]

void LLGLTexture::forceActive()
{
    mTextureState = ACTIVE ;
}

void LLGLTexture::setActive()
{
    if(mTextureState != NO_DELETE)
    {
        mTextureState = ACTIVE ;
    }
}

//set the texture to stay in memory
void LLGLTexture::setNoDelete()
{
    mTextureState = NO_DELETE ;
}

void LLGLTexture::generateGLTexture()
{
    if(mGLTexturep.isNull())
    {
        mGLTexturep = new LLImageGL(mFullWidth, mFullHeight, mComponents, mUseMipMaps) ;
    }
#ifdef DX_RENDER
    if(mDXTexturep.isNull())
    {
        mDXTexturep = new LLImageDX(mFullWidth, mFullHeight, mComponents, mUseMipMaps) ;
    }
#endif
}

LLImageGL* LLGLTexture::getGLTexture() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep ;
}

#ifdef DX_RENDER
LLImageDX* LLGLTexture::getDXTexture() const
{
    return mDXTexturep ;
}
#endif

bool LLGLTexture::createGLTexture()
{
    if(mGLTexturep.isNull())
    {
        generateGLTexture() ;
    }

#ifdef DX_RENDER
    if(mDXTexturep.isNull())
    {
        generateGLTexture() ;
    }
    return mDXTexturep.notNull() && mDXTexturep->createGLTexture() ;
#else
    return mGLTexturep->createGLTexture() ;
#endif
}

bool LLGLTexture::createGLTexture(S32 discard_level, const LLImageRaw* imageraw, S32 usename, bool to_create, S32 category, bool defer_copy, LLGLuint* tex_name)
{
    llassert(mGLTexturep.notNull());

#ifdef DX_RENDER
    if(mDXTexturep.isNull())
    {
        generateGLTexture() ;
    }
    bool ret = mDXTexturep.notNull() && mDXTexturep->createGLTexture(discard_level, imageraw, usename, to_create, category, defer_copy, tex_name) ;

    if(ret)
    {
        mFullWidth = mDXTexturep->getCurrentWidth() ;
        mFullHeight = mDXTexturep->getCurrentHeight() ;
        mComponents = mDXTexturep->getComponents() ;
        setTexelsPerImage();
    }
#else
    bool ret = mGLTexturep->createGLTexture(discard_level, imageraw, usename, to_create, category, defer_copy, tex_name) ;

    if(ret)
    {
        mFullWidth = mGLTexturep->getCurrentWidth() ;
        mFullHeight = mGLTexturep->getCurrentHeight() ;
        mComponents = mGLTexturep->getComponents() ;
        setTexelsPerImage();
    }
#endif

    return ret ;
}

void LLGLTexture::setExplicitFormat(LLGLint internal_format, LLGLenum primary_format, LLGLenum type_format, bool swap_bytes)
{
    llassert(mGLTexturep.notNull()) ;

    mGLTexturep->setExplicitFormat(internal_format, primary_format, type_format, swap_bytes) ;
}
void LLGLTexture::setAddressMode(LLTexUnit::eTextureAddressMode mode)
{
    llassert(mGLTexturep.notNull()) ;
    mGLTexturep->setAddressMode(mode) ;
#ifdef DX_RENDER
    if (mDXTexturep.notNull())
    {
        mDXTexturep->setAddressMode(mode) ;
    }
#endif
}
void LLGLTexture::setFilteringOption(LLTexUnit::eTextureFilterOptions option)
{
    llassert(mGLTexturep.notNull()) ;
    mGLTexturep->setFilteringOption(option) ;
#ifdef DX_RENDER
    if (mDXTexturep.notNull())
    {
        mDXTexturep->setFilteringOption(option) ;
    }
#endif
}

//virtual
S32 LLGLTexture::getWidth(S32 discard_level) const
{
#ifdef DX_RENDER
    llassert(mDXTexturep.notNull());
    return mDXTexturep->getWidth(discard_level);
#else
    llassert(mGLTexturep.notNull()) ;
    return mGLTexturep->getWidth(discard_level) ;
#endif
}

//virtual
S32 LLGLTexture::getHeight(S32 discard_level) const
{
#ifdef DX_RENDER
    llassert(mDXTexturep.notNull());
    return mDXTexturep->getHeight(discard_level);
#else
    llassert(mGLTexturep.notNull()) ;
    return mGLTexturep->getHeight(discard_level) ;
#endif
}

S32 LLGLTexture::getMaxDiscardLevel() const
{
#ifdef DX_RENDER
    llassert(mDXTexturep.notNull());
    return mDXTexturep->getMaxDiscardLevel();
#else
    llassert(mGLTexturep.notNull()) ;
    return mGLTexturep->getMaxDiscardLevel() ;
#endif
}
S32 LLGLTexture::getDiscardLevel() const
{
#ifdef DX_RENDER
    llassert(mDXTexturep.notNull());
    return mDXTexturep->getDiscardLevel();
#else
    llassert(mGLTexturep.notNull()) ;
    return mGLTexturep->getDiscardLevel() ;
#endif
}
S8  LLGLTexture::getComponents() const
{
#ifdef DX_RENDER
    llassert(mDXTexturep.notNull());
    return mDXTexturep->getComponents();
#else
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getComponents() ;
#endif
}

LLGLuint LLGLTexture::getTexName() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getTexName() ;
}

bool LLGLTexture::hasGLTexture() const
{
    if(mGLTexturep.notNull())
    {
        return mGLTexturep->getHasGLTexture() ;
    }
    return false ;
}

bool LLGLTexture::getBoundRecently() const
{
    if(mGLTexturep.notNull())
    {
        return mGLTexturep->getBoundRecently() ;
    }
    return false ;
}

LLTexUnit::eTextureType LLGLTexture::getTarget(void) const
{
    llassert(mGLTexturep.notNull()) ;
    return mGLTexturep->getTarget() ;
}

bool LLGLTexture::setSubImage(const LLImageRaw* imageraw, S32 x_pos, S32 y_pos, S32 width, S32 height, LLGLuint use_name)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->setSubImage(imageraw, x_pos, y_pos, width, height, 0, use_name) ;
}

bool LLGLTexture::setSubImage(const U8* datap, S32 data_width, S32 data_height, S32 x_pos, S32 y_pos, S32 width, S32 height, LLGLuint use_name)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->setSubImage(datap, data_width, data_height, x_pos, y_pos, width, height, 0, use_name) ;
}

void LLGLTexture::setGLTextureCreated (bool initialized)
{
    llassert(mGLTexturep.notNull()) ;

    mGLTexturep->setGLTextureCreated (initialized) ;
}

void  LLGLTexture::setCategory(S32 category)
{
    llassert(mGLTexturep.notNull()) ;

    mGLTexturep->setCategory(category) ;
}

void LLGLTexture::setTexName(LLGLuint texName)
{
    llassert(mGLTexturep.notNull());
    return mGLTexturep->setTexName(texName);
}

void LLGLTexture::setTarget(const LLGLenum target, const LLTexUnit::eTextureType bind_target)
{
    llassert(mGLTexturep.notNull());
    return mGLTexturep->setTarget(target, bind_target);
}

LLTexUnit::eTextureAddressMode LLGLTexture::getAddressMode(void) const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getAddressMode() ;
}

S32Bytes LLGLTexture::getTextureMemory() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->mTextureMemory ;
}

LLGLenum LLGLTexture::getPrimaryFormat() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getPrimaryFormat() ;
}

bool LLGLTexture::getIsAlphaMask() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getIsAlphaMask() ;
}

//bool LLGLTexture::getMask(const LLVector2 &tc)
// [RLVa:KB] - Checked: RLVa-2.2 (@setoverlay)
bool LLGLTexture::getMask(const LLVector2 &tc) const
// [/RLVa:KB]
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getMask(tc) ;
}

F32 LLGLTexture::getTimePassedSinceLastBound()
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getTimePassedSinceLastBound() ;
}
bool LLGLTexture::getMissed() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getMissed() ;
}

bool LLGLTexture::isJustBound() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->isJustBound() ;
}

void LLGLTexture::forceUpdateBindStats(void) const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->forceUpdateBindStats() ;
}

bool LLGLTexture::isGLTextureCreated() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->isGLTextureCreated() ;
}

void LLGLTexture::destroyGLTexture()
{
    if(mGLTexturep.notNull() && mGLTexturep->getHasGLTexture())
    {
        mGLTexturep->destroyGLTexture() ;
        mTextureState = DELETED ;
    }
}

void LLGLTexture::setTexelsPerImage()
{
    U32 fullwidth = llmin(mFullWidth,U32(MAX_IMAGE_SIZE_DEFAULT));
    U32 fullheight = llmin(mFullHeight,U32(MAX_IMAGE_SIZE_DEFAULT));
    mTexelsPerImage = (U32)fullwidth * fullheight;
}

static LLUUID sStubUUID;

const LLUUID& LLGLTexture::getID() const { return sStubUUID; }
