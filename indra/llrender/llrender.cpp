 /**
 * @file llrender.cpp
 * @brief LLRender implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#include "llrender.h"

#include "llvertexbuffer.h"
#include "llcubemap.h"
#include "llglslshader.h"
#include "llimagegl.h"
#include "llrendertarget.h"
#include "lltexture.h"
#include "llshadermgr.h"
#include "hbxxh.h"
#include "glm/gtc/type_ptr.hpp"

#ifdef DX_RENDER
// LLHLSLShader for syncMatricesDX()'s DX_RENDER branch - the DX-side sibling
// of the LLGLSLShader above, and the only shader class that has anything to
// upload matrices to under DX_RENDER. DX-only header, so guarded here and
// invisible to a DX_RENDER=OFF build.
#include "llhlslshader.h"
#include "DXSampler.h"
#include "DXStateCache.h"
#include "DXTexture.h"
#include "DXDevice.h"
#include "DXUIBatch.h"
#include "DXCubeMap.h"
#include "DXCubeMapArray.h"
#endif

#if LL_WINDOWS
extern void APIENTRY gl_debug_callback(GLenum source,
                                GLenum type,
                                GLuint id,
                                GLenum severity,
                                GLsizei length,
                                const GLchar* message,
                                GLvoid* userParam)
;
#endif

thread_local LLRender gGL;

#ifdef DX_RENDER
// The DX_RENDER render context, alongside (never in place of) gGL above - see
// llrender.h's own comment on the pair. Only the DX_RENDER llrender files
// reference it; nothing in the GL path does, so a DX_RENDER=OFF build neither
// declares nor instantiates it.
thread_local LLRender gDX;
#endif

// Handy copies of last good GL matrices
F32 gGLModelView[16];
F32 gGLLastModelView[16];
F32 gGLLastProjection[16];
F32 gGLProjection[16];

// transform from last frame's camera space to this frame's camera space (and inverse)
glm::mat4 gGLDeltaModelView;
glm::mat4 gGLInverseDeltaModelView;

S32 gGLViewport[4];

#ifdef DX_RENDER
// DX_RENDER counterpart of gGLViewport above. Defined (not just declared in
// llrender.h) here because that header is included by GL-only translation units
// too; guarding the definition keeps the symbol out of a DX_RENDER=OFF
// llrender.lib entirely. llrendertarget.cpp's glViewport() restore and
// newview/llviewerwindow.cpp both read/write the GL array only, so the two can
// never alias.
S32 gDXViewport[4];
#endif


U32 LLRender::sUICalls = 0;
U32 LLRender::sUIVerts = 0;
U32 LLTexUnit::sWhiteTexture = 0;
bool LLRender::sGLCoreProfile = false;
bool LLRender::sNsightDebugSupport = false;
LLVector2 LLRender::sUIGLScaleFactor = LLVector2(1.f, 1.f);
bool LLRender::sClassicMode = false;

struct LLVBCache
{
    LLPointer<LLVertexBuffer> vb;
    std::chrono::steady_clock::time_point touched;
};

static std::unordered_map<U64, LLVBCache> sVBCache;
static thread_local std::list<LLVertexBufferData> *sBufferDataList = nullptr;

static const GLenum sGLTextureType[] =
{
    GL_TEXTURE_2D,
    GL_TEXTURE_RECTANGLE,
    GL_TEXTURE_CUBE_MAP,
    GL_TEXTURE_CUBE_MAP_ARRAY,
    GL_TEXTURE_2D_MULTISAMPLE,
    GL_TEXTURE_3D
};

static const GLint sGLAddressMode[] =
{
    GL_REPEAT,
    GL_MIRRORED_REPEAT,
    GL_CLAMP_TO_EDGE
};

const U32 immediate_mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_COLOR | LLVertexBuffer::MAP_TEXCOORD0;

static const GLenum sGLBlendFactor[] =
{
    GL_ONE,
    GL_ZERO,
    GL_DST_COLOR,
    GL_SRC_COLOR,
    GL_ONE_MINUS_DST_COLOR,
    GL_ONE_MINUS_SRC_COLOR,
    GL_DST_ALPHA,
    GL_SRC_ALPHA,
    GL_ONE_MINUS_DST_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA,

    GL_ZERO // 'BF_UNDEF'
};

#ifdef DX_RENDER
// Same order/indexing as sGLBlendFactor above - LLRender::eBlendFactor maps onto
// D3D11_BLEND 1:1 (see DXStateCache.h), so this table is a straight translation
// of the same eleven slots rather than a separate mapping.
static const D3D11_BLEND sDXBlendFactor[] =
{
    D3D11_BLEND_ONE,
    D3D11_BLEND_ZERO,
    D3D11_BLEND_DEST_COLOR,
    D3D11_BLEND_SRC_COLOR,
    D3D11_BLEND_INV_DEST_COLOR,
    D3D11_BLEND_INV_SRC_COLOR,
    D3D11_BLEND_DEST_ALPHA,
    D3D11_BLEND_SRC_ALPHA,
    D3D11_BLEND_INV_DEST_ALPHA,
    D3D11_BLEND_INV_SRC_ALPHA,

    D3D11_BLEND_ZERO // 'BF_UNDEF'
};

namespace
{
    // DX_RENDER stand-in for GL's LLTexUnit::sWhiteTexture. Shaders such as
    // interface/uiF.hlsl unconditionally do
    // `vertex_color * diffuseMap.Sample(...)` for every 2D UI draw, textured or
    // not (solid-colour rects, borders and highlights included), and a null SRV
    // samples as (0,0,0,0) in HLSL rather than as a neutral no-op - so the DX
    // bind paths below must always bind *something*. A real 1x1 white
    // DXTexture, created once and reused, mirrors GL's sWhiteTexture role
    // exactly.
    ID3D11ShaderResourceView* getWhiteTextureSRV()
    {
        static DXTexture sWhiteDXTexture;
        static bool sInitialized = false;
        if (!sInitialized)
        {
            const uint8_t white_rgba[4] = { 255, 255, 255, 255 };
            sInitialized = sWhiteDXTexture.create(white_rgba, 1, 1, 4);
        }
        return sWhiteDXTexture.getSRV();
    }
}
#endif

LLTexUnit::LLTexUnit(S32 index)
    : mCurrTexType(TT_NONE),
    mCurrTexture(0),
    mHasMipMaps(false),
    mIndex(index)
{
    llassert_always(index < (S32)LL_NUM_TEXTURE_LAYERS);
}

//static
U32 LLTexUnit::getInternalType(eTextureType type)
{
    return sGLTextureType[type];
}

void LLTexUnit::refreshState(void)
{
    // We set dirty to true so that the tex unit knows to ignore caching
    // and we reset the cached tex unit state

    gGL.flush();

    glActiveTexture(GL_TEXTURE0 + mIndex);

    if (mCurrTexType != TT_NONE)
    {
        glBindTexture(sGLTextureType[mCurrTexType], mCurrTexture);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void LLTexUnit::activate(void)
{
    if (mIndex < 0) return;

    if ((S32)gGL.mCurrTextureUnitIndex != mIndex || gGL.mDirty)
    {
        gGL.flush();
        glActiveTexture(GL_TEXTURE0 + mIndex);
        gGL.mCurrTextureUnitIndex = mIndex;
    }
}

void LLTexUnit::enable(eTextureType type)
{
    if (mIndex < 0) return;

    if ( (mCurrTexType != type || gGL.mDirty) && (type != TT_NONE) )
    {
        activate();
        if (mCurrTexType != TT_NONE && !gGL.mDirty)
        {
            disable(); // Force a disable of a previous texture type if it's enabled.
        }
        mCurrTexType = type;

        gGL.flush();
    }
}

void LLTexUnit::disable(void)
{
    if (mIndex < 0) return;

    if (mCurrTexType != TT_NONE)
    {
        unbind(mCurrTexType);
        mCurrTexType = TT_NONE;
    }
}

void LLTexUnit::bindFast(LLTexture* texture)
{
    LLImageGL* gl_tex = texture->getGLTexture();
    texture->setActive();
    glActiveTexture(GL_TEXTURE0 + mIndex);
    gGL.mCurrTextureUnitIndex = mIndex;
    mCurrTexture = gl_tex->getTexName();
    if (!mCurrTexture)
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("MISSING TEXTURE");
        //if deleted, will re-generate it immediately
        texture->forceImmediateUpdate();
        gl_tex->forceUpdateBindStats();
        texture->bindDefaultImage(mIndex);
    }
    glBindTexture(sGLTextureType[gl_tex->getTarget()], mCurrTexture);
    mHasMipMaps = gl_tex->mHasMipMaps;
    if (gl_tex->mTexOptionsDirty)
    {
        gl_tex->mTexOptionsDirty = false;
        setTextureAddressModeFast(gl_tex->mAddressMode, gl_tex->getTarget());
        setTextureFilteringOptionFast(gl_tex->mFilterOption, gl_tex->getTarget());
    }
}

bool LLTexUnit::bind(LLTexture* texture, bool for_rendering, bool forceBind)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    stop_glerror();
    if (mIndex >= 0)
    {
        gGL.flush();

        LLImageGL* gl_tex = NULL ;

        if (texture != NULL && (gl_tex = texture->getGLTexture()))
        {
            if (gl_tex->getTexName()) //if texture exists
            {
                //in audit, replace the selected texture by the default one.
                if ((mCurrTexture != gl_tex->getTexName()) || forceBind)
                {
                    activate();
                    enable(gl_tex->getTarget());
                    mCurrTexture = gl_tex->getTexName();
                    glBindTexture(sGLTextureType[gl_tex->getTarget()], mCurrTexture);
                    if(gl_tex->updateBindStats())
                    {
                        texture->setActive() ;
                        texture->updateBindStatsForTester() ;
                    }
                    mHasMipMaps = gl_tex->mHasMipMaps;
                    if (gl_tex->mTexOptionsDirty)
                    {
                        gl_tex->mTexOptionsDirty = false;
                        setTextureAddressMode(gl_tex->mAddressMode);
                        setTextureFilteringOption(gl_tex->mFilterOption);
                    }
                }
            }
            else
            {
                //if deleted, will re-generate it immediately
                texture->forceImmediateUpdate() ;

                gl_tex->forceUpdateBindStats() ;
                return texture->bindDefaultImage(mIndex);
            }
        }
        else
        {
            if (texture)
            {
                LL_DEBUGS() << "NULL LLTexUnit::bind GL image" << LL_ENDL;
            }
            else
            {
                LL_DEBUGS() << "NULL LLTexUnit::bind texture" << LL_ENDL;
            }
            return false;
        }
    }
    else
    { // mIndex < 0
        return false;
    }

    return true;
}

bool LLTexUnit::bind(LLImageGL* texture, bool for_rendering, bool forceBind, S32 usename)
{
    stop_glerror();
    if (mIndex < 0) return false;

    U32 texname = usename ? usename : texture->getTexName();

    if(!texture)
    {
        LL_DEBUGS() << "NULL LLTexUnit::bind texture" << LL_ENDL;
        return false;
    }

    if(!texname)
    {
        if(LLImageGL::sDefaultGLTexture && LLImageGL::sDefaultGLTexture->getTexName())
        {
            return bind(LLImageGL::sDefaultGLTexture) ;
        }
        stop_glerror();
        return false ;
    }

    if ((mCurrTexture != texname) || forceBind)
    {
        gGL.flush();
        stop_glerror();
        activate();
        stop_glerror();
        enable(texture->getTarget());
        stop_glerror();
        mCurrTexture = texname;
        glBindTexture(sGLTextureType[texture->getTarget()], mCurrTexture);
        stop_glerror();
        texture->updateBindStats();
        mHasMipMaps = texture->mHasMipMaps;
        if (texture->mTexOptionsDirty)
        {
            stop_glerror();
            texture->mTexOptionsDirty = false;
            setTextureAddressMode(texture->mAddressMode);
            setTextureFilteringOption(texture->mFilterOption);
            stop_glerror();
        }
    }

    stop_glerror();

    return true;
}

bool LLTexUnit::bind(LLCubeMap* cubeMap)
{
    if (mIndex < 0) return false;

    gGL.flush();

    if (cubeMap == NULL)
    {
        LL_WARNS() << "NULL LLTexUnit::bind cubemap" << LL_ENDL;
        return false;
    }

    if (mCurrTexture != cubeMap->mImages[0]->getTexName())
    {
        if (LLCubeMap::sUseCubeMaps)
        {
            activate();
            enable(LLTexUnit::TT_CUBE_MAP);
            mCurrTexture = cubeMap->mImages[0]->getTexName();
            glBindTexture(GL_TEXTURE_CUBE_MAP, mCurrTexture);
            mHasMipMaps = cubeMap->mImages[0]->mHasMipMaps;
            cubeMap->mImages[0]->updateBindStats();
            if (cubeMap->mImages[0]->mTexOptionsDirty)
            {
                cubeMap->mImages[0]->mTexOptionsDirty = false;
                setTextureAddressMode(cubeMap->mImages[0]->mAddressMode);
                setTextureFilteringOption(cubeMap->mImages[0]->mFilterOption);
            }
            return true;
        }
        else
        {
            LL_WARNS() << "Using cube map without extension!" << LL_ENDL;
            return false;
        }
    }
    return true;
}

#ifdef DX_RENDER
// A unit's mIndex is derived from the GL enableTexture() bookkeeping on the GL
// path, which reports -1 (and getTexUnit() maps out-of-range indices onto
// mDummyTexUnit, whose index is also -1) for any texture-enabled channel DX has
// no equivalent of. A *hardcoded* valid index - e.g. gDX.getTexUnit(0)->bind(),
// used by converted pools that have no per-material channel registration yet -
// bypasses that guard, so mIndex must be checked here rather than relied upon.
bool LLTexUnit::bind(LLImageDX* texture, bool for_rendering, bool forceBind, S32 usename)
{
    if (mIndex < 0 || !texture) return false;

    // bound_image_changed catches SRV-address reuse from
    // LLImageDX::scaleDown() that mCurrDXSRV alone would miss, and a null SRV
    // falls back to white rather than sampling as (0,0,0,0).
    bool bound_image_changed = (mCurrBoundImageDX != texture);
    ID3D11ShaderResourceView* srv = texture->mDXTexture.getSRV();
    if (!srv)
    {
        srv = getWhiteTextureSRV();
    }
    bool srv_changed = bound_image_changed || mCurrDXSRV != (void*)srv;
    bool generation_stale = mDXSRVGeneration != DXStateCache::getRTVGeneration();
    if (srv_changed)
    {
        // Flush BEFORE updating mCurrBoundImageDX/mCurrDXSRV, not after -
        // otherwise LLRender::flush()'s mDXImage capture would tag
        // still-queued vertices (e.g. from LLFontVertexBuffer's
        // beginList()/endList() recording) with the NEW texture instead of the
        // one they were queued under.
        gDX.flush();
        gDXUIBatch.flushPending();
        mCurrDXSRV = (void*)srv;
    }
    mDXSRVGeneration = DXStateCache::getRTVGeneration();
    mCurrBoundImageDX = texture;
    ID3D11SamplerState* sampler = DXSampler::getOrCreate(
        (int)texture->getAddressMode(), (int)texture->getFilteringOption());
    if (srv_changed || generation_stale)
    {
        gDXDevice.getContext()->PSSetShaderResources(mIndex, 1, &srv);
    }
    // Sampler slots cap at 16, SRV slots do not.
    if (mIndex < 16 && mCurrDXSampler != (void*)sampler)
    {
        gDXDevice.getContext()->PSSetSamplers(mIndex, 1, &sampler);
    }
    mCurrDXSampler = (void*)sampler;
    return true;
}

bool LLTexUnit::bind(DXCubeMap* cubeMap)
{
    if (mIndex < 0 || !cubeMap || !DXCubeMap::sUseCubeMaps) return false;

    // cubeMap->getDXSRV() is null until init() has assembled all six faces, in
    // which case fall back to white rather than sampling as (0,0,0,0).
    ID3D11ShaderResourceView* srv = cubeMap->getDXSRV();
    if (!srv)
    {
        srv = getWhiteTextureSRV();
    }
    bool srv_changed = mCurrDXSRV != (void*)srv;
    bool generation_stale = mDXSRVGeneration != DXStateCache::getRTVGeneration();
    if (srv_changed)
    {
        gDX.flush();
        gDXUIBatch.flushPending();
        mCurrDXSRV = (void*)srv;
    }
    mDXSRVGeneration = DXStateCache::getRTVGeneration();
    // CLAMP + TRILINEAR - avoids seams at face edges - over the full mip chain
    // DXCubeTexture::create() always generates.
    ID3D11SamplerState* sampler = DXSampler::getOrCreate(2, 2);
    if (srv_changed || generation_stale)
    {
        gDXDevice.getContext()->PSSetShaderResources(mIndex, 1, &srv);
    }
    // Sampler slots cap at 16, SRV slots do not.
    if (mIndex < 16 && mCurrDXSampler != (void*)sampler)
    {
        gDXDevice.getContext()->PSSetSamplers(mIndex, 1, &sampler);
    }
    mCurrDXSampler = (void*)sampler;
    return true;
}

// Sibling of bind(DXCubeMap*) above for the array resource type
// (DXCubeArrayTexture) rather than the single-cubemap one, so there is no
// sUseCubeMaps-style static gate to check.
bool LLTexUnit::bind(DXCubeMapArray* cubeMapArray)
{
    if (mIndex < 0 || !cubeMapArray) return false;

    ID3D11ShaderResourceView* srv = cubeMapArray->getDXSRV();
    if (!srv)
    {
        srv = getWhiteTextureSRV();
    }
    bool srv_changed = mCurrDXSRV != (void*)srv;
    bool generation_stale = mDXSRVGeneration != DXStateCache::getRTVGeneration();
    if (srv_changed)
    {
        gDX.flush();
        gDXUIBatch.flushPending();
        mCurrDXSRV = (void*)srv;
    }
    mDXSRVGeneration = DXStateCache::getRTVGeneration();
    // CLAMP + TRILINEAR - same convention as bind(DXCubeMap*) above.
    ID3D11SamplerState* sampler = DXSampler::getOrCreate(2, 2);
    if (srv_changed || generation_stale)
    {
        gDXDevice.getContext()->PSSetShaderResources(mIndex, 1, &srv);
    }
    // Sampler slots cap at 16, SRV slots do not.
    if (mIndex < 16 && mCurrDXSampler != (void*)sampler)
    {
        gDXDevice.getContext()->PSSetSamplers(mIndex, 1, &sampler);
    }
    mCurrDXSampler = (void*)sampler;
    return true;
}
#endif

// LLRenderTarget is unavailible on the mapserver since it uses FBOs.
bool LLTexUnit::bind(LLRenderTarget* renderTarget, bool bindDepth)
{
    if (mIndex < 0) return false;

    gGL.flush();

    if (bindDepth)
    {
        llassert(renderTarget->getDepth()); // target MUST have a depth buffer attachment

        bindManual(renderTarget->getUsage(), renderTarget->getDepth());
    }
    else
    {
        bindManual(renderTarget->getUsage(), renderTarget->getTexture());
    }

    return true;
}

bool LLTexUnit::bindManual(eTextureType type, U32 texture, bool hasMips)
{
    if (mIndex < 0)
    {
        return false;
    }

    if(mCurrTexture != texture)
    {
        gGL.flush();

        activate();
        enable(type);
        mCurrTexture = texture;
        glBindTexture(sGLTextureType[type], texture);
        mHasMipMaps = hasMips;
    }
    return true;
}

void LLTexUnit::unbind(eTextureType type)
{
    stop_glerror();

    if (mIndex < 0) return;

    //always flush and activate for consistency
    //   some code paths assume unbind always flushes and sets the active texture
    gGL.flush();
    activate();

    // Disabled caching of binding state.
    if (mCurrTexType == type)
    {
        mCurrTexture = 0;

        if (type == LLTexUnit::TT_TEXTURE)
        {
            glBindTexture(sGLTextureType[type], sWhiteTexture);
        }
        else
        {
            glBindTexture(sGLTextureType[type], 0);
        }
        stop_glerror();
    }
}

void LLTexUnit::unbindFast(eTextureType type)
{
    activate();

    // Disabled caching of binding state.
    if (mCurrTexType == type)
    {
        mCurrTexture = 0;

        if (type == LLTexUnit::TT_TEXTURE)
        {
            glBindTexture(sGLTextureType[type], sWhiteTexture);
        }
        else
        {
            glBindTexture(sGLTextureType[type], 0);
        }
    }
}

void LLTexUnit::setTextureAddressMode(eTextureAddressMode mode)
{
    if (mIndex < 0 || mCurrTexture == 0) return;

    gGL.flush();

    activate();

    setTextureAddressModeFast(mode, mCurrTexType);
}

void LLTexUnit::setTextureAddressModeFast(eTextureAddressMode mode, eTextureType tex_type)
{
    glTexParameteri(sGLTextureType[tex_type], GL_TEXTURE_WRAP_S, sGLAddressMode[mode]);
    glTexParameteri(sGLTextureType[tex_type], GL_TEXTURE_WRAP_T, sGLAddressMode[mode]);
    if (tex_type == TT_CUBE_MAP || tex_type == TT_CUBE_MAP_ARRAY || tex_type == TT_TEXTURE_3D)
    {
        glTexParameteri(sGLTextureType[tex_type], GL_TEXTURE_WRAP_R, sGLAddressMode[mode]);
    }
}

void LLTexUnit::setTextureFilteringOption(LLTexUnit::eTextureFilterOptions option)
{
    if (mIndex < 0 || mCurrTexture == 0 || mCurrTexType == LLTexUnit::TT_MULTISAMPLE_TEXTURE) return;

    gGL.flush();

    setTextureFilteringOptionFast(option, mCurrTexType);
}

void LLTexUnit::setTextureFilteringOptionFast(LLTexUnit::eTextureFilterOptions option, eTextureType tex_type)
{
    if (option == TFO_POINT)
    {
        glTexParameteri(sGLTextureType[tex_type], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    {
        glTexParameteri(sGLTextureType[tex_type], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    if (option >= TFO_TRILINEAR && mHasMipMaps)
    {
        glTexParameteri(sGLTextureType[tex_type], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    else if (option >= TFO_BILINEAR)
    {
        if (mHasMipMaps)
        {
            glTexParameteri(sGLTextureType[tex_type], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        }
        else
        {
            glTexParameteri(sGLTextureType[tex_type], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
    }
    else
    {
        if (mHasMipMaps)
        {
            glTexParameteri(sGLTextureType[tex_type], GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        }
        else
        {
            glTexParameteri(sGLTextureType[tex_type], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }
    }

    if (gGLManager.mHasAnisotropic)
    {
        if (LLImageGL::sGlobalUseAnisotropic && option == TFO_ANISOTROPIC)
        {
            glTexParameterf(sGLTextureType[tex_type], GL_TEXTURE_MAX_ANISOTROPY, gGLManager.mMaxAnisotropy);
        }
        else
        {
            glTexParameterf(sGLTextureType[tex_type], GL_TEXTURE_MAX_ANISOTROPY, 1.f);
        }
    }
}

GLint LLTexUnit::getTextureSource(eTextureBlendSrc src)
{
    switch(src)
    {
        // All four cases should return the same value.
        case TBS_PREV_COLOR:
        case TBS_PREV_ALPHA:
        case TBS_ONE_MINUS_PREV_COLOR:
        case TBS_ONE_MINUS_PREV_ALPHA:
            return GL_PREVIOUS;

        // All four cases should return the same value.
        case TBS_TEX_COLOR:
        case TBS_TEX_ALPHA:
        case TBS_ONE_MINUS_TEX_COLOR:
        case TBS_ONE_MINUS_TEX_ALPHA:
            return GL_TEXTURE;

        // All four cases should return the same value.
        case TBS_VERT_COLOR:
        case TBS_VERT_ALPHA:
        case TBS_ONE_MINUS_VERT_COLOR:
        case TBS_ONE_MINUS_VERT_ALPHA:
            return GL_PRIMARY_COLOR;

        // All four cases should return the same value.
        case TBS_CONST_COLOR:
        case TBS_CONST_ALPHA:
        case TBS_ONE_MINUS_CONST_COLOR:
        case TBS_ONE_MINUS_CONST_ALPHA:
            return GL_CONSTANT;

        default:
            LL_WARNS() << "Unknown eTextureBlendSrc: " << src << ".  Using Vertex Color instead." << LL_ENDL;
            return GL_PRIMARY_COLOR;
    }
}

GLint LLTexUnit::getTextureSourceType(eTextureBlendSrc src, bool isAlpha)
{
    switch(src)
    {
        // All four cases should return the same value.
        case TBS_PREV_COLOR:
        case TBS_TEX_COLOR:
        case TBS_VERT_COLOR:
        case TBS_CONST_COLOR:
            return (isAlpha) ? GL_SRC_ALPHA: GL_SRC_COLOR;

        // All four cases should return the same value.
        case TBS_PREV_ALPHA:
        case TBS_TEX_ALPHA:
        case TBS_VERT_ALPHA:
        case TBS_CONST_ALPHA:
            return GL_SRC_ALPHA;

        // All four cases should return the same value.
        case TBS_ONE_MINUS_PREV_COLOR:
        case TBS_ONE_MINUS_TEX_COLOR:
        case TBS_ONE_MINUS_VERT_COLOR:
        case TBS_ONE_MINUS_CONST_COLOR:
            return (isAlpha) ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE_MINUS_SRC_COLOR;

        // All four cases should return the same value.
        case TBS_ONE_MINUS_PREV_ALPHA:
        case TBS_ONE_MINUS_TEX_ALPHA:
        case TBS_ONE_MINUS_VERT_ALPHA:
        case TBS_ONE_MINUS_CONST_ALPHA:
            return GL_ONE_MINUS_SRC_ALPHA;

        default:
            LL_WARNS() << "Unknown eTextureBlendSrc: " << src << ".  Using Source Color or Alpha instead." << LL_ENDL;
            return (isAlpha) ? GL_SRC_ALPHA: GL_SRC_COLOR;
    }
}

// Useful for debugging that you've manually assigned a texture operation to the correct
// texture unit based on the currently set active texture in opengl.
void LLTexUnit::debugTextureUnit(void)
{
    if (mIndex < 0) return;

    GLint activeTexture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    if ((GL_TEXTURE0 + mIndex) != activeTexture)
    {
        U32 set_unit = (activeTexture - GL_TEXTURE0);
        LL_WARNS() << "Incorrect Texture Unit!  Expected: " << set_unit << " Actual: " << mIndex << LL_ENDL;
    }
}

LLLightState::LLLightState(S32 index)
: mIndex(index),
  mEnabled(false),
  mConstantAtten(1.f),
  mLinearAtten(0.f),
  mQuadraticAtten(0.f),
  mSpotExponent(0.f),
  mSpotCutoff(180.f)
{
    if (mIndex == 0)
    {
        mDiffuse.set(1,1,1,1);
        mDiffuseB.set(0,0,0,0);
        mSpecular.set(1,1,1,1);
    }

    mSunIsPrimary = true;

    mAmbient.set(0,0,0,1);
    mPosition.set(0,0,1,0);
    mSpotDirection.set(0,0,-1);
}

void LLLightState::enable()
{
    mEnabled = true;
}

void LLLightState::disable()
{
    mEnabled = false;
}

void LLLightState::setDiffuse(const LLColor4& diffuse)
{
    if (mDiffuse != diffuse)
    {
        ++gGL.mLightHash;
        mDiffuse = diffuse;
    }
}

void LLLightState::setDiffuseB(const LLColor4& diffuse)
{
    if (mDiffuseB != diffuse)
    {
        ++gGL.mLightHash;
        mDiffuseB = diffuse;
    }
}

void LLLightState::setSunPrimary(bool v)
{
    if (mSunIsPrimary != v)
    {
        ++gGL.mLightHash;
        mSunIsPrimary = v;
    }
}

void LLLightState::setSize(F32 v)
{
    if (mSize != v)
    {
        ++gGL.mLightHash;
        mSize = v;
    }
}

void LLLightState::setFalloff(F32 v)
{
    if (mFalloff != v)
    {
        ++gGL.mLightHash;
        mFalloff = v;
    }
}

void LLLightState::setAmbient(const LLColor4& ambient)
{
    if (mAmbient != ambient)
    {
        ++gGL.mLightHash;
        mAmbient = ambient;
    }
}

void LLLightState::setSpecular(const LLColor4& specular)
{
    if (mSpecular != specular)
    {
        ++gGL.mLightHash;
        mSpecular = specular;
    }
}

void LLLightState::setPosition(const LLVector4& position)
{
    //always set position because modelview matrix may have changed
    ++gGL.mLightHash;
    mPosition = position;
    //transform position by current modelview matrix
    glm::vec4 pos(position);
    pos = gGL.getModelviewMatrix() * pos;
    mPosition.set(glm::value_ptr(pos));
}

void LLLightState::setConstantAttenuation(const F32& atten)
{
    if (mConstantAtten != atten)
    {
        mConstantAtten = atten;
        ++gGL.mLightHash;
    }
}

void LLLightState::setLinearAttenuation(const F32& atten)
{
    if (mLinearAtten != atten)
    {
        ++gGL.mLightHash;
        mLinearAtten = atten;
    }
}

void LLLightState::setQuadraticAttenuation(const F32& atten)
{
    if (mQuadraticAtten != atten)
    {
        ++gGL.mLightHash;
        mQuadraticAtten = atten;
    }
}

void LLLightState::setSpotExponent(const F32& exponent)
{
    if (mSpotExponent != exponent)
    {
        ++gGL.mLightHash;
        mSpotExponent = exponent;
    }
}

void LLLightState::setSpotCutoff(const F32& cutoff)
{
    if (mSpotCutoff != cutoff)
    {
        ++gGL.mLightHash;
        mSpotCutoff = cutoff;
    }
}

void LLLightState::setSpotDirection(const LLVector3& direction)
{
    //always set direction because modelview matrix may have changed
    ++gGL.mLightHash;

    //transform direction by current modelview matrix
    glm::vec3 dir(direction);
    const glm::mat3 mat(gGL.getModelviewMatrix());
    dir = mat * dir;

    mSpotDirection.set(glm::value_ptr(dir));
}

LLRender::LLRender()
  : mDirty(false),
    mCount(0),
    mMode(LLRender::TRIANGLES),
    mCurrTextureUnitIndex(0),
    mLineWidth(1.f), // <FS> Line width OGL core profile fix by Rye Mutt
    // <FS:Ansariel> Don't ignore OpenGL max line width
    mMaxLineWidthSmooth(1.f),
    mMaxLineWidthAliased(1.f)
    // </FS:Ansariel>
{
    for (U32 i = 0; i < LL_NUM_TEXTURE_LAYERS; i++)
    {
        mTexUnits[i].mIndex = i;
    }

    for (U32 i = 0; i < LL_NUM_LIGHT_UNITS; ++i)
    {
        mLightState[i].mIndex = i;
    }

    for (U32 i = 0; i < 4; i++)
    {
        mCurrColorMask[i] = true;
    }

    mCurrBlendColorSFactor = BF_UNDEF;
    mCurrBlendAlphaSFactor = BF_UNDEF;
    mCurrBlendColorDFactor = BF_UNDEF;
    mCurrBlendAlphaDFactor = BF_UNDEF;

    mMatrixMode = LLRender::MM_MODELVIEW;

    for (U32 i = 0; i < NUM_MATRIX_MODES; ++i)
    {
        for (U32 j = 0; j < LL_MATRIX_STACK_DEPTH; ++j)
        {
            mMatrix[i][j] = glm::identity<glm::mat4>();
        }
        mMatIdx[i] = 0;
        mMatHash[i] = 0;
        mCurMatHash[i] = 0xFFFFFFFF;
    }

    mLightHash = 0;
}

LLRender::~LLRender()
{
    shutdown();
}

bool LLRender::init(bool needs_vertex_buffer)
{
#if LL_WINDOWS
    if (gGLManager.mHasDebugOutput && gDebugGL)
    { //setup debug output callback
        //glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW_ARB, 0, NULL, GL_TRUE);
        glDebugMessageCallback((GLDEBUGPROC) gl_debug_callback, NULL);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    }
#endif

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    gGL.setSceneBlendType(LLRender::BT_ALPHA);
    gGL.setAmbientLightColor(LLColor4::black);

    glCullFace(GL_BACK);
#ifdef DX_RENDER
    // glCullFace() above is a statically linked core-GL symbol with no DX11
    // dispatch-table entry, so it does nothing under DX_RENDER and
    // DXState::sCullFace stays at its GL_BACK default anyway. cullFace() is
    // the cross-backend spelling that additionally tracks the direction and
    // binds the D3D11 rasterizer state, so route the same intent through it
    // here. GL is untouched: the call above still runs, unchanged.
    cullFace(GL_BACK);
#endif

    // necessary for reflection maps
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

#if LL_WINDOWS
    if (glGenVertexArrays == nullptr)
    {
        return false;
    }
#endif

    { //bind a dummy vertex array object so we're core profile compliant
        U32 ret;
        glGenVertexArrays(1, &ret);
        glBindVertexArray(ret);
    }

    if (needs_vertex_buffer)
    {
        initVertexBuffer();
    }

    // <FS:Ansariel> Don't ignore OpenGL max line width
    GLfloat range[2];
    glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, range);
    stop_glerror();
    mMaxLineWidthAliased = range[1];
    glGetFloatv(GL_SMOOTH_LINE_WIDTH_RANGE, range);
    stop_glerror();
    mMaxLineWidthSmooth = range[1];
    // </FS:Ansariel>

    return true;
}

void LLRender::initVertexBuffer()
{
    llassert_always(mBuffer.isNull()) ;
    stop_glerror();
    mBuffer = new LLVertexBuffer(immediate_mask);
    // <FS:Ansariel> Warn in case of allocation failure
    //mBuffer->allocateBuffer(4096, 0);
    if (!mBuffer->allocateBuffer(4096, 0))
    {
        // If this doesn't work, we're knee-deep in trouble!
        LL_WARNS() << "Failed to allocate Vertex Buffer for common rendering" << LL_ENDL;
    }
    mBuffer->getVertexStrider(mVerticesp);
    mBuffer->getTexCoord0Strider(mTexcoordsp);
    mBuffer->getColorStrider(mColorsp);
    stop_glerror();
}

void LLRender::resetVertexBuffer()
{
    mBuffer = NULL;
}

void LLRender::shutdown()
{
    resetVertexBuffer();
}

void LLRender::refreshState(void)
{
    mDirty = true;

    U32 active_unit = mCurrTextureUnitIndex;

    for (U32 i = 0; i < mTexUnits.size(); i++)
    {
        mTexUnits[i].refreshState();
    }

    mTexUnits[active_unit].activate();

    setColorMask(mCurrColorMask[0], mCurrColorMask[1], mCurrColorMask[2], mCurrColorMask[3]);

    flush();

    mDirty = false;
}

void LLRender::syncLightState()
{
    LLGLSLShader *shader = LLGLSLShader::sCurBoundShaderPtr;

    if (!shader)
    {
        return;
    }

    if (shader->mLightHash != mLightHash)
    {
        shader->mLightHash = mLightHash;

        LLVector4 position[LL_NUM_LIGHT_UNITS];
        LLVector3 direction[LL_NUM_LIGHT_UNITS];
        LLVector4 attenuation[LL_NUM_LIGHT_UNITS];
        LLVector3 diffuse[LL_NUM_LIGHT_UNITS];
        LLVector3 diffuse_b[LL_NUM_LIGHT_UNITS];
        bool      sun_primary[LL_NUM_LIGHT_UNITS];
        LLVector2 size[LL_NUM_LIGHT_UNITS];

        for (U32 i = 0; i < LL_NUM_LIGHT_UNITS; i++)
        {
            LLLightState *light = &mLightState[i];

            position[i]  = light->mPosition;
            direction[i] = light->mSpotDirection;
            attenuation[i].set(light->mLinearAtten, light->mQuadraticAtten, light->mSpecular.mV[2], light->mSpecular.mV[3]);
            diffuse[i].set(light->mDiffuse.mV);
            diffuse_b[i].set(light->mDiffuseB.mV);
            sun_primary[i] = light->mSunIsPrimary;
            size[i].set(light->mSize, light->mFalloff);
        }

        shader->uniform4fv(LLShaderMgr::LIGHT_POSITION, LL_NUM_LIGHT_UNITS, position[0].mV);
        shader->uniform3fv(LLShaderMgr::LIGHT_DIRECTION, LL_NUM_LIGHT_UNITS, direction[0].mV);
        shader->uniform4fv(LLShaderMgr::LIGHT_ATTENUATION, LL_NUM_LIGHT_UNITS, attenuation[0].mV);
        shader->uniform2fv(LLShaderMgr::LIGHT_DEFERRED_ATTENUATION, LL_NUM_LIGHT_UNITS, size[0].mV);
        shader->uniform3fv(LLShaderMgr::LIGHT_DIFFUSE, LL_NUM_LIGHT_UNITS, diffuse[0].mV);
        shader->uniform3fv(LLShaderMgr::LIGHT_AMBIENT, 1, mAmbientLightColor.mV);
        shader->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_primary[0] ? 1 : 0);

        if (sClassicMode)
        {
            shader->uniform3fv(LLShaderMgr::AMBIENT, 1, mAmbientLightColor.mV);
            shader->uniform3fv(LLShaderMgr::SUNLIGHT_COLOR, 1, diffuse[0].mV);
            shader->uniform3fv(LLShaderMgr::MOONLIGHT_COLOR, 1, diffuse_b[0].mV);
        }
    }
}

#ifdef DX_RENDER
namespace
{
    // Extracts the upper-left 3x3 (as 3 columns of 3 floats each) from a
    // column-major mat4 - matches the GL path's own norm_mat[] construction
    // in syncMatrices() below (glm::value_ptr(mat)[0,1,2],[4,5,6],[8,9,10]).
    void extractMat3(const glm::mat4& mat, float* out3x3)
    {
        const float* m = glm::value_ptr(mat);
        out3x3[0] = m[0]; out3x3[1] = m[1]; out3x3[2] = m[2];
        out3x3[3] = m[4]; out3x3[4] = m[5]; out3x3[5] = m[6];
        out3x3[6] = m[8]; out3x3[7] = m[9]; out3x3[8] = m[10];
    }
}
#endif

void LLRender::syncMatrices()
{
    STOP_GLERROR;
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;

#ifdef DX_RENDER
    syncMatricesDX();
#endif

    static const U32 name[] =
    {
        LLShaderMgr::MODELVIEW_MATRIX,
        LLShaderMgr::PROJECTION_MATRIX,
        LLShaderMgr::TEXTURE_MATRIX0,
        LLShaderMgr::TEXTURE_MATRIX1,
        LLShaderMgr::TEXTURE_MATRIX2,
        LLShaderMgr::TEXTURE_MATRIX3,
    };

    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;

    static glm::mat4 cached_mvp;
    static glm::mat4 cached_inv_mdv;
    static U32 cached_mvp_mdv_hash = 0xFFFFFFFF;
    static U32 cached_mvp_proj_hash = 0xFFFFFFFF;

    static glm::mat4 cached_normal;
    static U32 cached_normal_hash = 0xFFFFFFFF;

    if (shader)
    {
        bool mvp_done = false;

        U32 i = MM_MODELVIEW;
        if (mMatHash[MM_MODELVIEW] != shader->mMatHash[MM_MODELVIEW])
        { //update modelview, normal, and MVP
            const glm::mat4& mat = mMatrix[MM_MODELVIEW][mMatIdx[MM_MODELVIEW]];

            // if MDV has changed, update the cached inverse as well
            if (cached_mvp_mdv_hash != mMatHash[MM_MODELVIEW])
            {
                cached_inv_mdv = glm::inverse(mat);
            }

            shader->uniformMatrix4fv(name[MM_MODELVIEW], 1, GL_FALSE, glm::value_ptr(mat));
            shader->mMatHash[MM_MODELVIEW] = mMatHash[MM_MODELVIEW];

            //update normal matrix
            S32 loc = shader->getUniformLocation(LLShaderMgr::NORMAL_MATRIX);
            if (loc > -1)
            {
                if (cached_normal_hash != mMatHash[i])
                {
                    cached_normal = glm::transpose(cached_inv_mdv);
                    cached_normal_hash = mMatHash[i];
                }

                auto norm = glm::value_ptr(cached_normal);

                F32 norm_mat[] =
                {
                    norm[0], norm[1], norm[2],
                    norm[4], norm[5], norm[6],
                    norm[8], norm[9], norm[10]
                };

                shader->uniformMatrix3fv(LLShaderMgr::NORMAL_MATRIX, 1, GL_FALSE, norm_mat);
            }

            if (shader->getUniformLocation(LLShaderMgr::INVERSE_MODELVIEW_MATRIX))
            {
                shader->uniformMatrix4fv(LLShaderMgr::INVERSE_MODELVIEW_MATRIX, 1, GL_FALSE, glm::value_ptr(cached_inv_mdv));
            }

            //update MVP matrix
            mvp_done = true;
            loc = shader->getUniformLocation(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX);
            if (loc > -1)
            {
                U32 proj = MM_PROJECTION;

                if (cached_mvp_mdv_hash != mMatHash[i] || cached_mvp_proj_hash != mMatHash[MM_PROJECTION])
                {
                    cached_mvp = mat;
                    cached_mvp = mMatrix[proj][mMatIdx[proj]] * cached_mvp;
                    cached_mvp_mdv_hash = mMatHash[i];
                    cached_mvp_proj_hash = mMatHash[MM_PROJECTION];
                }

                shader->uniformMatrix4fv(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX, 1, GL_FALSE, glm::value_ptr(cached_mvp));
            }
        }

        i = MM_PROJECTION;
        if (mMatHash[MM_PROJECTION] != shader->mMatHash[MM_PROJECTION])
        { //update projection matrix, normal, and MVP
            const glm::mat4& mat = mMatrix[MM_PROJECTION][mMatIdx[MM_PROJECTION]];

            // GZ: This was previously disabled seemingly due to a bug involving the deferred renderer's regular pushing and popping of mats.
            // We're reenabling this and cleaning up the code around that - that would've been the appropriate course initially.
            // Anything beyond the standard proj and inv proj mats are special cases.  Please setup special uniforms accordingly in the future.
            if (shader->getUniformLocation(LLShaderMgr::INVERSE_PROJECTION_MATRIX))
            {
                glm::mat4 inv_proj = glm::inverse(mat);
                shader->uniformMatrix4fv(LLShaderMgr::INVERSE_PROJECTION_MATRIX, 1, false, glm::value_ptr(inv_proj));
            }

            // Used by some full screen effects - such as full screen lights, glow, etc.
            if (shader->getUniformLocation(LLShaderMgr::IDENTITY_MATRIX))
            {
                shader->uniformMatrix4fv(LLShaderMgr::IDENTITY_MATRIX, 1, GL_FALSE, glm::value_ptr(glm::identity<glm::mat4>()));
            }

            shader->uniformMatrix4fv(name[MM_PROJECTION], 1, GL_FALSE, glm::value_ptr(mat));
            shader->mMatHash[MM_PROJECTION] = mMatHash[MM_PROJECTION];

            if (!mvp_done)
            {
                //update MVP matrix
                S32 loc = shader->getUniformLocation(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX);
                if (loc > -1)
                {
                    if (cached_mvp_mdv_hash != mMatHash[MM_PROJECTION] || cached_mvp_proj_hash != mMatHash[MM_PROJECTION])
                    {
                        U32 mdv = MM_MODELVIEW;
                        cached_mvp = mat;
                        cached_mvp *= mMatrix[mdv][mMatIdx[mdv]];
                        cached_mvp_mdv_hash = mMatHash[MM_MODELVIEW];
                        cached_mvp_proj_hash = mMatHash[MM_PROJECTION];
                    }

                    shader->uniformMatrix4fv(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX, 1, GL_FALSE, glm::value_ptr(cached_mvp));
                }
            }
        }

        for (i = MM_TEXTURE0; i < NUM_MATRIX_MODES; ++i)
        {
            if (mMatHash[i] != shader->mMatHash[i])
            {
                shader->uniformMatrix4fv(name[i], 1, GL_FALSE, glm::value_ptr(mMatrix[i][mMatIdx[i]]));
                shader->mMatHash[i] = mMatHash[i];
            }
        }


        if (shader->mFeatures.hasLighting || shader->mFeatures.calculatesLighting || shader->mFeatures.calculatesAtmospherics)
        { //also sync light state
            syncLightState();
        }
    }
    STOP_GLERROR;
}

#ifdef DX_RENDER
// DX_RENDER counterpart of the matrix upload syncMatrices() does above, into
// the LLHLSLShader/DXShader pair that LLHLSLShader::bind() makes current
// instead of an LLGLSLShader. Only pushes the matrices syncMatrices() itself is
// responsible for - modelview/projection/normal/texture0 plus the inverse
// projection several deferred passes need - not GL's full inverse-modelview /
// texture1-3 set.
void LLRender::syncMatricesDX()
{
    LLHLSLShader* dx_shader = LLHLSLShader::sCurBoundShaderPtr;
    if (!dx_shader)
    {
        return;
    }

    DXShader& vs = dx_shader->mDXVertexShader;
    DXShader& ps = dx_shader->mDXPixelShader;

    // Skip the matrix math and the setUniformMatrix4/3 calls below when nothing
    // relevant changed since THIS shader's last sync. mMatHash[mode] is a
    // monotonic per-mode counter bumped by every real matrix mutator
    // (loadMatrix/multMatrix/loadIdentity/popMatrix/translatef/etc below), so it
    // cannot miss a real camera switch. vs.uploadConstants()/
    // VSSetConstantBuffers()/ps.uploadConstants()/PSSetConstantBuffers() further
    // down stay unconditional - still needed to flush any other pending uniform
    // write and rebind the buffer after a shader switch.
    bool matrices_changed =
        (mMatHash[MM_MODELVIEW] != dx_shader->mMatHash[MM_MODELVIEW]) ||
        (mMatHash[MM_PROJECTION] != dx_shader->mMatHash[MM_PROJECTION]) ||
        (mMatHash[MM_TEXTURE0] != dx_shader->mMatHash[MM_TEXTURE0]);

    if (matrices_changed)
    {
        const glm::mat4& mdv = mMatrix[MM_MODELVIEW][mMatIdx[MM_MODELVIEW]];

        // GL-convention projection matrices (glm::frustum()/ortho()/
        // perspective(), e.g. LLViewerCamera::calcProjection()'s
        // glm::perspective() - GLM_FORCE_DEPTH_ZERO_TO_ONE is not defined
        // anywhere in this project) produce clip-space z in [-w,w], i.e. NDC z
        // in [-1,1] after the divide. D3D11 requires clip-space z in [0,w] (NDC
        // z in [0,1]) and clips away anything outside that range - fed a raw
        // GL-convention matrix, the near half of the intended frustum (NDC z in
        // [-1,0)) gets clipped as "in front of the near plane". Remapped here
        // rather than touching the shared GL-convention projection-matrix
        // construction code, which the GL build still depends on unmodified.
        //
        // z'=0.5*w-0.5*z (not the standard z'=0.5*z+0.5*w) - reversed-Z, so
        // stored depth is near=1.0/far=0.0. This matrix is every pass's
        // projection upload chokepoint (shadows included), so this one sign flip
        // is the root of the whole conversion. Every site that turns a stored
        // depth value back into GL NDC z must flip its own `2.0*depth-1.0` to
        // `1.0-2.0*depth` - see deferredUtil.hlsl/aoUtil.hlsl/waterF.hlsl, which
        // already do. Comparison funcs (glDepthFuncToDX(), llgl.cpp) and clear
        // values (DXRenderTarget.cpp/DXContext.cpp, 1.0f->0.0f) flip alongside
        // this as part of the same conversion, and all three are already in this
        // tree - so this is the last of the three, not a new convention.
        static const glm::mat4 kGLtoDXDepthRemap = []()
        {
            glm::mat4 m(1.0f);
            m[2][2] = -0.5f;
            m[3][2] = 0.5f;
            return m;
        }();
        const glm::mat4& raw_proj = mMatrix[MM_PROJECTION][mMatIdx[MM_PROJECTION]];
        const glm::mat4 proj = kGLtoDXDepthRemap * raw_proj;
        glm::mat4 mvp = proj * mdv;
        float normal3x3[9];
        extractMat3(glm::transpose(glm::inverse(mdv)), normal3x3);

        vs.setUniformMatrix4("modelview_matrix", glm::value_ptr(mdv));
        vs.setUniformMatrix4("modelview_projection_matrix", glm::value_ptr(mvp));
        // Standalone "projection_matrix" (not just the combined
        // modelview_projection_matrix above) is needed by every HAS_SKIN/
        // rigged vertex shader, which build an eye-space position via
        // skin+modelview before projecting separately for lighting/normal math.
        // Uses the same D3D11-depth-remapped `proj` (not raw_proj) as
        // modelview_projection_matrix, since it feeds SV_Position.
        vs.setUniformMatrix4("projection_matrix", glm::value_ptr(proj));
        vs.setUniformMatrix3("normal_matrix", normal3x3);
        const glm::mat4& tex_mat0 = mMatrix[MM_TEXTURE0][mMatIdx[MM_TEXTURE0]];
        vs.setUniformMatrix4("texture_matrix0", glm::value_ptr(tex_mat0));

        // inv_proj uses the UN-remapped GL-convention projection matrix, not
        // `proj` above - getPositionWithDepth() (deferredUtil.hlsl) manually
        // converts D3D11 [0,1] depth back to GL's [-1,1] NDC before this
        // multiply, so inv_proj must invert that same GL-convention matrix.
        // Pushed to both vs and ps - setUniformMatrix4() no-ops harmlessly
        // wherever a stage doesn't declare "inv_proj".
        glm::mat4 inv_proj = glm::inverse(raw_proj);
        vs.setUniformMatrix4("inv_proj", glm::value_ptr(inv_proj));
        ps.setUniformMatrix4("inv_proj", glm::value_ptr(inv_proj));

        // Remember what this shader was just synced with, so the next draw using
        // it can detect "nothing changed" and skip this block.
        dx_shader->mMatHash[MM_MODELVIEW] = mMatHash[MM_MODELVIEW];
        dx_shader->mMatHash[MM_PROJECTION] = mMatHash[MM_PROJECTION];
        dx_shader->mMatHash[MM_TEXTURE0] = mMatHash[MM_TEXTURE0];
    }

    // Gated the same way the GL body of syncMatrices() gates its own
    // syncLightState() call: only for shaders that actually declare
    // lighting/atmospherics, not unconditionally for every bind (UI, water,
    // post-process shaders never needed it).
    if (dx_shader->mFeatures.hasLighting || dx_shader->mFeatures.calculatesLighting || dx_shader->mFeatures.calculatesAtmospherics)
    {
        syncLightStateDX();
    }

    vs.uploadConstants();

    // Bind to $Globals' real reflected slot, not a hardcoded 0 - a shader
    // claiming b0 for its own named cbuffer (e.g. pbrmetallicroughnessV.hlsl's
    // GLTFMaterials) pushes $Globals to b1 instead. See
    // DXShader::getConstantBufferBindPoint().
    ID3D11Buffer* cb = vs.getConstantBuffer();
    if (cb)
    {
        gDXDevice.getContext()->VSSetConstantBuffers(vs.getConstantBufferBindPoint(), 1, &cb);
    }

    // Symmetric pixel-shader handling - solidcolorF.hlsl's "uniform vec4 color"
    // needs this (see LLHLSLShader::uniform4f()'s DX_RENDER branch); without
    // it, staged pixel-stage uniforms never reach the GPU. No-ops for shaders
    // with no pixel-stage top-level uniforms (mDXPixelShader.getConstantBuffer()
    // returns nullptr).
    ps.uploadConstants();
    if (ID3D11Buffer* pcb = ps.getConstantBuffer())
    {
        gDXDevice.getContext()->PSSetConstantBuffers(ps.getConstantBufferBindPoint(), 1, &pcb);
    }
}

// syncLightState() above is GL-only: it reads LLGLSLShader::sCurBoundShaderPtr,
// which under DX_RENDER is never set (LLGLSLShader::bind() is not on the DX
// path at all), so it returns immediately and light_position[]/
// light_direction[]/light_attenuation[]/light_diffuse[]/ambient/sun_up_factor
// never reach a shader - leaving forward-lit alpha surfaces (avatar hair,
// alpha-blend/cutout clothing) unlit. This is that same upload against the
// shader that actually is bound under DX_RENDER. Kept as a separate member
// rather than a DX_RENDER branch inside syncLightState() itself so the GL
// function is left byte-for-byte alone; it reads the same private
// mLightState/mAmbientLightColor, hence a member rather than a file-static.
void LLRender::syncLightStateDX()
{
    LLHLSLShader* shader = LLHLSLShader::sCurBoundShaderPtr;
    if (!shader)
    {
        return;
    }

    if (shader->mLightHash != mLightHash)
    {
        shader->mLightHash = mLightHash;

        LLVector4 position[LL_NUM_LIGHT_UNITS];
        LLVector3 direction[LL_NUM_LIGHT_UNITS];
        LLVector4 attenuation[LL_NUM_LIGHT_UNITS];
        LLVector3 diffuse[LL_NUM_LIGHT_UNITS];
        LLVector3 diffuse_b[LL_NUM_LIGHT_UNITS];
        bool      sun_primary[LL_NUM_LIGHT_UNITS];
        LLVector2 size[LL_NUM_LIGHT_UNITS];

        for (U32 i = 0; i < LL_NUM_LIGHT_UNITS; i++)
        {
            LLLightState *light = &mLightState[i];

            position[i]  = light->mPosition;
            direction[i] = light->mSpotDirection;
            attenuation[i].set(light->mLinearAtten, light->mQuadraticAtten, light->mSpecular.mV[2], light->mSpecular.mV[3]);
            diffuse[i].set(light->mDiffuse.mV);
            diffuse_b[i].set(light->mDiffuseB.mV);
            sun_primary[i] = light->mSunIsPrimary;
            size[i].set(light->mSize, light->mFalloff);
        }

        shader->uniform4fv(LLShaderMgr::LIGHT_POSITION, LL_NUM_LIGHT_UNITS, position[0].mV);
        shader->uniform3fv(LLShaderMgr::LIGHT_DIRECTION, LL_NUM_LIGHT_UNITS, direction[0].mV);
        shader->uniform4fv(LLShaderMgr::LIGHT_ATTENUATION, LL_NUM_LIGHT_UNITS, attenuation[0].mV);
        shader->uniform2fv(LLShaderMgr::LIGHT_DEFERRED_ATTENUATION, LL_NUM_LIGHT_UNITS, size[0].mV);
        shader->uniform3fv(LLShaderMgr::LIGHT_DIFFUSE, LL_NUM_LIGHT_UNITS, diffuse[0].mV);
        shader->uniform3fv(LLShaderMgr::LIGHT_AMBIENT, 1, mAmbientLightColor.mV);
        shader->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_primary[0] ? 1 : 0);

        if (sClassicMode)
        {
            shader->uniform3fv(LLShaderMgr::AMBIENT, 1, mAmbientLightColor.mV);
            shader->uniform3fv(LLShaderMgr::SUNLIGHT_COLOR, 1, diffuse[0].mV);
            shader->uniform3fv(LLShaderMgr::MOONLIGHT_COLOR, 1, diffuse_b[0].mV);
        }
    }
}
#endif

void LLRender::translatef(const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
    flush();

    {
        mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] = glm::translate(mMatrix[mMatrixMode][mMatIdx[mMatrixMode]], glm::vec3(x, y, z));
        mMatHash[mMatrixMode]++;
    }
}

void LLRender::scalef(const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
    flush();

    {
        mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] = glm::scale(mMatrix[mMatrixMode][mMatIdx[mMatrixMode]], glm::vec3(x, y, z));
        mMatHash[mMatrixMode]++;
    }
}

void LLRender::ortho(F32 left, F32 right, F32 bottom, F32 top, F32 zNear, F32 zFar)
{
    flush();

    {
        mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] *= glm::ortho(left, right, bottom, top, zNear, zFar);
        mMatHash[mMatrixMode]++;
    }
}

void LLRender::rotatef(const GLfloat& a, const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
    flush();

    {
        mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] = glm::rotate(mMatrix[mMatrixMode][mMatIdx[mMatrixMode]], glm::radians(a), glm::vec3(x,y,z));
        mMatHash[mMatrixMode]++;
    }
}

void LLRender::pushMatrix()
{
    flush();

    {
        if (mMatIdx[mMatrixMode] < LL_MATRIX_STACK_DEPTH-1)
        {
            mMatrix[mMatrixMode][mMatIdx[mMatrixMode]+1] = mMatrix[mMatrixMode][mMatIdx[mMatrixMode]];
            ++mMatIdx[mMatrixMode];
        }
        else
        {
            LL_WARNS() << "Matrix stack overflow." << LL_ENDL;
        }
    }
}

void LLRender::popMatrix()
{
    flush();
    {
        if (mMatIdx[mMatrixMode] > 0)
        {
            --mMatIdx[mMatrixMode];
            mMatHash[mMatrixMode]++;
        }
        else
        {
            LL_WARNS() << "Matrix stack underflow." << LL_ENDL;
        }
    }
}

void LLRender::loadMatrix(const GLfloat* m)
{
    flush();
    {
        mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] = glm::make_mat4((GLfloat*) m);
        mMatHash[mMatrixMode]++;
    }
}

void LLRender::multMatrix(const GLfloat* m)
{
    flush();
    {
        mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] *= glm::make_mat4(m);
        mMatHash[mMatrixMode]++;
    }
}

void LLRender::matrixMode(eMatrixMode mode)
{
    if (mode == MM_TEXTURE)
    {
        U32 tex_index = gGL.getCurrentTexUnitIndex();
        // the shaders don't actually reference anything beyond texture_matrix0/1 outside of terrain rendering
        llassert(tex_index <= 3);
        mode = eMatrixMode(MM_TEXTURE0 + tex_index);
        if (mode > MM_TEXTURE3)
        {
            // getCurrentTexUnitIndex() can go as high as 32 (LL_NUM_TEXTURE_LAYERS)
            // Large value will result in a crash at mMatrix
            LL_WARNS_ONCE() << "Attempted to assign matrix mode out of bounds: " << mode << LL_ENDL;
            mode = MM_TEXTURE0;
        }
    }

    mMatrixMode = mode;
}

LLRender::eMatrixMode LLRender::getMatrixMode()
{
    if (mMatrixMode >= MM_TEXTURE0 && mMatrixMode <= MM_TEXTURE3)
    { //always return MM_TEXTURE if current matrix mode points at any texture matrix
        return MM_TEXTURE;
    }

    return mMatrixMode;
}


void LLRender::loadIdentity()
{
    flush();

    {
        llassert_always(mMatrixMode < NUM_MATRIX_MODES) ;

        mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] = glm::identity<glm::mat4>();
        mMatHash[mMatrixMode]++;
    }
}

const glm::mat4& LLRender::getModelviewMatrix()
{
    return mMatrix[MM_MODELVIEW][mMatIdx[MM_MODELVIEW]];
}

const glm::mat4& LLRender::getProjectionMatrix()
{
    return mMatrix[MM_PROJECTION][mMatIdx[MM_PROJECTION]];
}

void LLRender::translateUI(F32 x, F32 y, F32 z)
{
    if (mUIOffset.empty())
    {
        LL_ERRS() << "Need to push a UI translation frame before offsetting" << LL_ENDL;
    }

    mUIOffset.back().add(LLVector4a(x, y, z));
}

void LLRender::scaleUI(F32 x, F32 y, F32 z)
{
    if (mUIScale.empty())
    {
        LL_ERRS() << "Need to push a UI transformation frame before scaling." << LL_ENDL;
    }

    mUIScale.back().mul(LLVector4a(x, y, z));
}

void LLRender::pushUIMatrix()
{
    if (mUIOffset.empty())
    {
        mUIOffset.emplace_back(0.f);
    }
    else
    {
        mUIOffset.push_back(mUIOffset.back());
    }

    if (mUIScale.empty())
    {
        mUIScale.emplace_back(1.f);
    }
    else
    {
        mUIScale.push_back(mUIScale.back());
    }
}

void LLRender::popUIMatrix()
{
    if (mUIOffset.empty())
    {
        LL_ERRS() << "UI offset stack blown." << LL_ENDL;
    }
    mUIOffset.pop_back();
    mUIScale.pop_back();
}

LLVector3 LLRender::getUITranslation()
{
    if (mUIOffset.empty())
    {
        return LLVector3::zero;
    }

    return LLVector3(mUIOffset.back().getF32ptr());
}

LLVector3 LLRender::getUIScale()
{
    if (mUIScale.empty())
    {
        return LLVector3::all_one;
    }

    return LLVector3(mUIScale.back().getF32ptr());
}


void LLRender::loadUIIdentity()
{
    if (mUIOffset.empty())
    {
        LL_ERRS() << "Need to push UI translation frame before clearing offset." << LL_ENDL;
    }

    mUIOffset.back().clear();
    mUIScale.back().splat(1);
}

void LLRender::setColorMask(bool writeColor, bool writeAlpha)
{
    setColorMask(writeColor, writeColor, writeColor, writeAlpha);
}

void LLRender::setColorMask(bool writeColorR, bool writeColorG, bool writeColorB, bool writeAlpha)
{
    flush();

    if (mCurrColorMask[0] != writeColorR ||
        mCurrColorMask[1] != writeColorG ||
        mCurrColorMask[2] != writeColorB ||
        mCurrColorMask[3] != writeAlpha)
    {
        mCurrColorMask[0] = writeColorR;
        mCurrColorMask[1] = writeColorG;
        mCurrColorMask[2] = writeColorB;
        mCurrColorMask[3] = writeAlpha;

        glColorMask(writeColorR ? GL_TRUE : GL_FALSE,
                    writeColorG ? GL_TRUE : GL_FALSE,
                    writeColorB ? GL_TRUE : GL_FALSE,
                    writeAlpha ? GL_TRUE : GL_FALSE);

#ifdef DX_RENDER
        // The color write mask is the third input D3D11 folds into the same
        // ID3D11BlendState as blend-enable and the blend factors, so it has to
        // go through the same chokepoint as blendFunc() above - otherwise the
        // D3D11 mask lags until the next blendFunc()/GL_BLEND toggle happens
        // to rebuild it. Inside the if() on purpose: an unchanged mask needs
        // no state push, exactly as the glColorMask() above is skipped.
        applyDXBlendState();
#endif
    }
}

void LLRender::setSceneBlendType(eBlendType type)
{
    switch (type)
    {
        case BT_ALPHA:
            blendFunc(BF_SOURCE_ALPHA, BF_ONE_MINUS_SOURCE_ALPHA);
            break;
        case BT_ADD:
            blendFunc(BF_ONE, BF_ONE);
            break;
        case BT_ADD_WITH_ALPHA:
            blendFunc(BF_SOURCE_ALPHA, BF_ONE);
            break;
        case BT_MULT:
            blendFunc(BF_DEST_COLOR, BF_ZERO);
            break;
        case BT_MULT_ALPHA:
            blendFunc(BF_DEST_ALPHA, BF_ZERO);
            break;
        case BT_MULT_X2:
            blendFunc(BF_DEST_COLOR, BF_SOURCE_COLOR);
            break;
        case BT_REPLACE:
            blendFunc(BF_ONE, BF_ZERO);
            break;
        default:
            LL_ERRS() << "Unknown Scene Blend Type: " << type << LL_ENDL;
            break;
    }
}

void LLRender::blendFunc(eBlendFactor sfactor, eBlendFactor dfactor)
{
    llassert(sfactor < BF_UNDEF);
    llassert(dfactor < BF_UNDEF);
    if (mCurrBlendColorSFactor != sfactor || mCurrBlendColorDFactor != dfactor ||
        mCurrBlendAlphaSFactor != sfactor || mCurrBlendAlphaDFactor != dfactor)
    {
        mCurrBlendColorSFactor = sfactor;
        mCurrBlendAlphaSFactor = sfactor;
        mCurrBlendColorDFactor = dfactor;
        mCurrBlendAlphaDFactor = dfactor;
        flush();
        glBlendFunc(sGLBlendFactor[sfactor], sGLBlendFactor[dfactor]);
    }

#ifdef DX_RENDER
    // D3D11 has no separate glBlendFunc(): the enable toggle, both factor pairs
    // and the color write mask are all one state object, so it has to be rebuilt
    // from every contributing input whenever any of them changes. This overload
    // sets the alpha factors equal to the color ones, so one call covers all
    // four - see applyDXBlendState()'s definition below.
    applyDXBlendState();
#endif
}

void LLRender::blendFunc(eBlendFactor color_sfactor, eBlendFactor color_dfactor,
             eBlendFactor alpha_sfactor, eBlendFactor alpha_dfactor)
{
    llassert(color_sfactor < BF_UNDEF);
    llassert(color_dfactor < BF_UNDEF);
    llassert(alpha_sfactor < BF_UNDEF);
    llassert(alpha_dfactor < BF_UNDEF);

    if (mCurrBlendColorSFactor != color_sfactor || mCurrBlendColorDFactor != color_dfactor ||
        mCurrBlendAlphaSFactor != alpha_sfactor || mCurrBlendAlphaDFactor != alpha_dfactor)
    {
        mCurrBlendColorSFactor = color_sfactor;
        mCurrBlendAlphaSFactor = alpha_sfactor;
        mCurrBlendColorDFactor = color_dfactor;
        mCurrBlendAlphaDFactor = alpha_dfactor;
        flush();

        glBlendFuncSeparate(sGLBlendFactor[color_sfactor], sGLBlendFactor[color_dfactor],
                           sGLBlendFactor[alpha_sfactor], sGLBlendFactor[alpha_dfactor]);
    }

#ifdef DX_RENDER
    applyDXBlendState();
#endif
}

#ifdef DX_RENDER
void LLRender::applyDXBlendState()
{
    uint8_t write_mask = 0;
    if (mCurrColorMask[0]) write_mask |= D3D11_COLOR_WRITE_ENABLE_RED;
    if (mCurrColorMask[1]) write_mask |= D3D11_COLOR_WRITE_ENABLE_GREEN;
    if (mCurrColorMask[2]) write_mask |= D3D11_COLOR_WRITE_ENABLE_BLUE;
    if (mCurrColorMask[3]) write_mask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;

    bool enabled = DXState::isEnabled(GL_BLEND);
    D3D11_BLEND src = sDXBlendFactor[mCurrBlendColorSFactor];
    D3D11_BLEND dst = sDXBlendFactor[mCurrBlendColorDFactor];
    // Passed through for real, not silently dropped - see
    // DXStateCache::getBlendState(). A no-op for the 2-factor blendFunc()
    // overload (which sets these equal to src/dst); it matters for the 4-factor
    // blendFuncSeparate() overload, which needs its own alpha factors.
    D3D11_BLEND alpha_src = sDXBlendFactor[mCurrBlendAlphaSFactor];
    D3D11_BLEND alpha_dst = sDXBlendFactor[mCurrBlendAlphaDFactor];

    ID3D11BlendState* bs = DXStateCache::getBlendState(enabled, src, dst, alpha_src, alpha_dst, write_mask);
    gDXDevice.getContext()->OMSetBlendState(bs, nullptr, 0xFFFFFFFF);
}

void LLRender::applyDXRasterizerState()
{
    bool offset_enabled = DXState::isEnabled(GL_POLYGON_OFFSET_FILL) || DXState::isEnabled(GL_POLYGON_OFFSET_LINE);
    ID3D11RasterizerState* rs = DXStateCache::getRasterizerState(
        DXState::isEnabled(GL_CULL_FACE),
        DXState::isEnabled(GL_SCISSOR_TEST),
        DXState::isEnabled(GL_DEPTH_CLAMP),
        offset_enabled ? mCurrPolygonOffsetFactor : 0.f,
        offset_enabled ? mCurrPolygonOffsetUnits : 0.f,
        DXStateCache::sWireframeScopeActive,
        DXState::getCullFace() == GL_FRONT);
    gDXDevice.getContext()->RSSetState(rs);
}
#endif

void LLRender::cullFace(GLenum face)
{
#ifndef DX_RENDER
    glCullFace(face);
#else
    DXState::setCullFace(face);
    applyDXRasterizerState();
#endif
}

void LLRender::setPolygonOffset(F32 factor, F32 units)
{
#ifndef DX_RENDER
    glPolygonOffset(factor, units);
#else
    mCurrPolygonOffsetFactor = factor;
    mCurrPolygonOffsetUnits = units;
    applyDXRasterizerState();
#endif
}

LLTexUnit* LLRender::getTexUnit(U32 index)
{
    if (index < mTexUnits.size())
    {
        return &mTexUnits[index];
    }
    else
    {
        LL_DEBUGS() << "Non-existing texture unit layer requested: " << index << LL_ENDL;
        return &mDummyTexUnit;
    }
}

LLLightState* LLRender::getLight(U32 index)
{
    if (index < mLightState.size())
    {
        return &mLightState[index];
    }

    return NULL;
}

void LLRender::setAmbientLightColor(const LLColor4& color)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    if (color != mAmbientLightColor)
    {
        ++mLightHash;
        mAmbientLightColor = color;
    }
}

// <FS> Line width OGL core profile fix by Rye Mutt
void LLRender::setLineWidth(F32 line_width)
{
    if (line_width > 1.f)
    {
        line_width = llmin(line_width, glIsEnabled(GL_LINE_SMOOTH) ? mMaxLineWidthSmooth : mMaxLineWidthAliased);
    }
    if (mLineWidth != line_width || mDirty)
    {
        if (mMode == LLRender::LINES || mMode == LLRender::LINE_STRIP)
        {
            flush();
        }
        mLineWidth = line_width;
        glLineWidth(line_width);
    }
}
// </FS>

bool LLRender::verifyTexUnitActive(U32 unitToVerify)
{
    if (mCurrTextureUnitIndex == unitToVerify)
    {
        return true;
    }
    else
    {
        LL_WARNS() << "TexUnit currently active: " << mCurrTextureUnitIndex << " (expecting " << unitToVerify << ")" << LL_ENDL;
        return false;
    }
}

void LLRender::clearErrors()
{
    while (glGetError())
    {
        //loop until no more error flags left
    }
}

void LLRender::beginList(std::list<LLVertexBufferData> *list)
{
    if (sBufferDataList)
    {
        LL_ERRS() << "beginList called while another list is open." << LL_ENDL;
    }
    llassert(LLGLSLShader::sCurBoundShaderPtr == &gUIProgram);
    flush();
    sBufferDataList = list;
}

void LLRender::endList()
{
    if (sBufferDataList)
    {
        flush();
        sBufferDataList = nullptr;
    }
    else
    {
        llassert(false); // endList called without an open list
    }
}

bool LLRender::isRecording() const
{
    return sBufferDataList != nullptr;
}

void LLRender::begin(const GLuint& mode)
{
    if (mode != mMode)
    {
        if (mMode == LLRender::LINES ||
            mMode == LLRender::TRIANGLES ||
            mMode == LLRender::POINTS)
        {
            flush();
        }
        else if (mCount != 0)
        {
            LL_ERRS() << "gGL.begin() called redundantly." << LL_ENDL;
        }

        mMode = mode;
    }
}

void LLRender::end()
{
    if (mCount == 0)
    {
        return;
        //IMM_ERRS << "GL begin and end called with no vertices specified." << LL_ENDL;
    }

    if ((mMode != LLRender::LINES &&
        mMode != LLRender::TRIANGLES &&
        mMode != LLRender::POINTS) ||
        mCount > 2048)
    {
        flush();
    }
}

void LLRender::flush()
{
    STOP_GLERROR;
    if (mCount > 0)
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
        llassert_always(LLGLSLShader::sCurBoundShaderPtr != nullptr);

        if (!mUIOffset.empty())
        {
            sUICalls++;
            sUIVerts += mCount;
        }

        //store mCount in a local variable to avoid re-entrance (drawArrays may call flush)
        U32 count = mCount;

        if (mMode == LLRender::TRIANGLES)
        {
            if (mCount%3 != 0)
            {
            count -= (mCount % 3);
            LL_WARNS() << "Incomplete triangle requested." << LL_ENDL;
            }
        }

        if (mMode == LLRender::LINES)
        {
            if (mCount%2 != 0)
            {
                count -= (mCount % 2);
                LL_WARNS() << "Incomplete line requested." << LL_ENDL;
            }
        }

        mCount = 0;

        if (mBuffer)
        {

            LLVertexBuffer *vb;

            U32 attribute_mask = LLGLSLShader::sCurBoundShaderPtr->mAttributeMask;

            if (sBufferDataList)
            {
                vb = genBuffer(attribute_mask, count);
                sBufferDataList->emplace_back(
                    vb,
                    mMode,
                    count,
                    gGL.getTexUnit(0)->mCurrTexture,
                    mMatrix[MM_MODELVIEW][mMatIdx[MM_MODELVIEW]],
                    mMatrix[MM_PROJECTION][mMatIdx[MM_PROJECTION]],
                    mMatrix[MM_TEXTURE0][mMatIdx[MM_TEXTURE0]]
                    );
            }
            else
            {
                vb = bufferfromCache(attribute_mask, count);
            }

            drawBuffer(vb, mMode, count);
        }
        else
        {
            // mBuffer is present in main thread and not present in an image thread
            LL_ERRS() << "A flush call from outside main rendering thread" << LL_ENDL;
        }

        resetStriders(count);
    }
}

LLVertexBuffer* LLRender::bufferfromCache(U32 attribute_mask, U32 count)
{
    LLVertexBuffer *vb = nullptr;
    HBXXH64 hash;

    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_VERTEX("vb cache hash");

        hash.update((U8*)mVerticesp.get(), count * sizeof(LLVector4a));
        if (attribute_mask & LLVertexBuffer::MAP_TEXCOORD0)
        {
            hash.update((U8*)mTexcoordsp.get(), count * sizeof(LLVector2));
        }

        if (attribute_mask & LLVertexBuffer::MAP_COLOR)
        {
            hash.update((U8*)mColorsp.get(), count * sizeof(LLColor4U));
        }

        hash.finalize();
    }

    U64 vhash = hash.digest();

    // check the VB cache before making a new vertex buffer
    // This is a giant hack to deal with (mostly) our terrible UI rendering code
    // that was built on top of OpenGL immediate mode.  Huge performance wins
    // can be had by not uploading geometry to VRAM unless absolutely necessary.
    // Most of our usage of the "immediate mode" style draw calls is actually
    // sending the same geometry over and over again.
    // To leverage this, we maintain a running hash of the vertex stream being
    // built up before a flush, and then check that hash against a VB
    // cache just before creating a vertex buffer in VRAM
    std::unordered_map<U64, LLVBCache>::iterator cache = sVBCache.find(vhash);

    if (cache != sVBCache.end())
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_VERTEX("vb cache hit");
        // cache hit, just use the cached buffer
        vb = cache->second.vb;
        cache->second.touched = std::chrono::steady_clock::now();
    }
    else
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_VERTEX("vb cache miss");
        vb = genBuffer(attribute_mask, count);

        sVBCache[vhash] = { vb , std::chrono::steady_clock::now() };

        static U32 miss_count = 0;
        miss_count++;
        if (miss_count > 1024)
        {
            LL_PROFILE_ZONE_NAMED_CATEGORY_VERTEX("vb cache clean");
            miss_count = 0;
            auto now = std::chrono::steady_clock::now();

            using namespace std::chrono_literals;
            // every 1024 misses, clean the cache of any VBs that haven't been touched in the last second
            for (std::unordered_map<U64, LLVBCache>::iterator iter = sVBCache.begin(); iter != sVBCache.end(); )
            {
                if (now - iter->second.touched > 1s)
                {
                    iter = sVBCache.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }
        }
    }
    return vb;
}

LLVertexBuffer* LLRender::genBuffer(U32 attribute_mask, S32 count)
{
    LLVertexBuffer * vb = new LLVertexBuffer(attribute_mask);
    vb->allocateBuffer(count, 0);

    vb->setBuffer();

    vb->setPositionData(mVerticesp.get());

    if (attribute_mask & LLVertexBuffer::MAP_TEXCOORD0)
    {
        vb->setTexCoord0Data(mTexcoordsp.get());
    }

    if (attribute_mask & LLVertexBuffer::MAP_COLOR)
    {
        vb->setColorData(mColorsp.get());
    }

#if LL_DARWIN
    vb->unmapBuffer();
#endif
    vb->unbind();

    return vb;
}

void LLRender::drawBuffer(LLVertexBuffer* vb, U32 mode, S32 count)
{
    vb->setBuffer();
    vb->drawArrays(mode, 0, count);
}

void LLRender::resetStriders(S32 count)
{
    mVerticesp[0] = mVerticesp[count];
    mTexcoordsp[0] = mTexcoordsp[count];
    mColorsp[0] = mColorsp[count];

    mCount = 0;
}

void LLRender::vertex3f(const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
    //the range of mVerticesp, mColorsp and mTexcoordsp is [0, 4095]
    if (mCount > 2048)
    { //break when buffer gets reasonably full to keep GL command buffers happy and avoid overflow below
        switch (mMode)
        {
            case LLRender::POINTS: flush(); break;
            case LLRender::TRIANGLES: if (mCount%3==0) flush(); break;
            case LLRender::LINES: if (mCount%2 == 0) flush(); break;
        }
    }

    if (mCount > 4094)
    {
    //  LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
        return;
    }

    LLVector4a vert(x, y, z);
    transform(vert);
    mVerticesp[mCount] = vert;

    mCount++;
    mVerticesp[mCount] = mVerticesp[mCount-1];
    mColorsp[mCount] = mColorsp[mCount-1];
    mTexcoordsp[mCount] = mTexcoordsp[mCount-1];
}

void LLRender::transform(LLVector3& vert)
{
    if (!mUIOffset.empty())
    {
        vert += LLVector3(mUIOffset.back().getF32ptr());
        vert *= LLVector3(mUIScale.back().getF32ptr());
    }
}

void LLRender::transform(LLVector4a& vert)
{
    if (!mUIOffset.empty())
    {
        vert.add(mUIOffset.back());
        vert.mul(mUIScale.back());
    }
}

void LLRender::untransform(LLVector3& vert)
{
    if (!mUIOffset.empty())
    {
        vert /= LLVector3(mUIScale.back().getF32ptr());
        vert -= LLVector3(mUIOffset.back().getF32ptr());
    }
}

void LLRender::batchTransform(LLVector4a* verts, U32 vert_count)
{
    if (!mUIOffset.empty())
    {
        const LLVector4a& offset = mUIOffset.back();
        const LLVector4a& scale = mUIScale.back();

        for (U32 i = 0; i < vert_count; ++i)
        {
            verts[i].add(offset);
            verts[i].mul(scale);
        }
    }
}

void LLRender::vertexBatchPreTransformed(const std::vector<LLVector4a>& verts)
{
    vertexBatchPreTransformed(verts.data(), narrow(verts.size()));
}

void LLRender::vertexBatchPreTransformed(const LLVector4a* verts, S32 vert_count)
{
    if (mCount + vert_count > 4094)
    {
        //  LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
        return;
    }

    for (S32 i = 0; i < vert_count; i++)
    {
        mVerticesp[mCount] = verts[i];

        mCount++;
        mTexcoordsp[mCount] = mTexcoordsp[mCount-1];
        mColorsp[mCount] = mColorsp[mCount-1];
    }

    if( mCount > 0 ) // ND: Guard against crashes if mCount is zero, yes it can happen
        mVerticesp[mCount] = mVerticesp[mCount-1];
}

void LLRender::vertexBatchPreTransformed(const LLVector4a* verts, const LLVector2* uvs, S32 vert_count)
{
    if (mCount + vert_count > 4094)
    {
        //  LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
        return;
    }

    for (S32 i = 0; i < vert_count; i++)
    {
        mVerticesp[mCount] = verts[i];
        mTexcoordsp[mCount] = uvs[i];

        mCount++;
        mColorsp[mCount] = mColorsp[mCount-1];
    }

    if (mCount > 0)
    {
        mVerticesp[mCount] = mVerticesp[mCount - 1];
        mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
    }
}

void LLRender::vertexBatchPreTransformed(const LLVector4a* verts, const LLVector2* uvs, const LLColor4U* colors, S32 vert_count)
{
    if (mCount + vert_count > 4094)
    {
        //  LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
        return;
    }

    for (S32 i = 0; i < vert_count; i++)
    {
        mVerticesp[mCount] = verts[i];
        mTexcoordsp[mCount] = uvs[i];
        mColorsp[mCount] = colors[i];

        mCount++;
    }

    if (mCount > 0)
    {
        mVerticesp[mCount] = mVerticesp[mCount - 1];
        mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
        mColorsp[mCount] = mColorsp[mCount - 1];
    }
}

void LLRender::vertex2i(const GLint& x, const GLint& y)
{
    vertex3f((GLfloat) x, (GLfloat) y, 0);
}

void LLRender::vertex2f(const GLfloat& x, const GLfloat& y)
{
    vertex3f(x,y,0);
}

void LLRender::vertex2fv(const GLfloat* v)
{
    vertex3f(v[0], v[1], 0);
}

void LLRender::vertex3fv(const GLfloat* v)
{
    vertex3f(v[0], v[1], v[2]);
}

void LLRender::texCoord2f(const GLfloat& x, const GLfloat& y)
{
    mTexcoordsp[mCount] = LLVector2(x,y);
}

void LLRender::texCoord2i(const GLint& x, const GLint& y)
{
    texCoord2f((GLfloat) x, (GLfloat) y);
}

void LLRender::texCoord2fv(const GLfloat* tc)
{
    texCoord2f(tc[0], tc[1]);
}

void LLRender::color4ub(const GLubyte& r, const GLubyte& g, const GLubyte& b, const GLubyte& a)
{
    if (!LLGLSLShader::sCurBoundShaderPtr || LLGLSLShader::sCurBoundShaderPtr->mAttributeMask & LLVertexBuffer::MAP_COLOR)
    {
        mColorsp[mCount] = LLColor4U(r,g,b,a);
    }
    else
    { //not using shaders or shader reads color from a uniform
        diffuseColor4ub(r,g,b,a);
    }
}
void LLRender::color4ubv(const GLubyte* c)
{
    color4ub(c[0], c[1], c[2], c[3]);
}

void LLRender::color4f(const GLfloat& r, const GLfloat& g, const GLfloat& b, const GLfloat& a)
{
    color4ub((GLubyte) (llclamp(r, 0.f, 1.f)*255),
        (GLubyte) (llclamp(g, 0.f, 1.f)*255),
        (GLubyte) (llclamp(b, 0.f, 1.f)*255),
        (GLubyte) (llclamp(a, 0.f, 1.f)*255));
}

void LLRender::color4fv(const GLfloat* c)
{
    color4f(c[0],c[1],c[2],c[3]);
}

void LLRender::color3f(const GLfloat& r, const GLfloat& g, const GLfloat& b)
{
    color4f(r,g,b,1);
}

void LLRender::color3fv(const GLfloat* c)
{
    color4f(c[0],c[1],c[2],1);
}

void LLRender::diffuseColor3f(F32 r, F32 g, F32 b)
{
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    llassert(shader != NULL);

    if (shader)
    {
        shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, r,g,b,1.f);
    }
}

void LLRender::diffuseColor3fv(const F32* c)
{
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    llassert(shader != NULL);

    if (shader)
    {
        shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, c[0], c[1], c[2], 1.f);
    }
}

void LLRender::diffuseColor4f(F32 r, F32 g, F32 b, F32 a)
{
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    llassert(shader != NULL);

    if (shader)
    {
        shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, r,g,b,a);
    }
}

void LLRender::diffuseColor4fv(const F32* c)
{
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    llassert(shader != NULL);

    if (shader)
    {
        shader->uniform4fv(LLShaderMgr::DIFFUSE_COLOR, 1, c);
    }
}

void LLRender::diffuseColor4ubv(const U8* c)
{
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    llassert(shader != NULL);

    if (shader)
    {
        shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, c[0]/255.f, c[1]/255.f, c[2]/255.f, c[3]/255.f);
    }
}

void LLRender::diffuseColor4ub(U8 r, U8 g, U8 b, U8 a)
{
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    llassert(shader != NULL);

    if (shader)
    {
        shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, r/255.f, g/255.f, b/255.f, a/255.f);
    }
}


void LLRender::debugTexUnits(void)
{
    LL_INFOS("TextureUnit") << "Active TexUnit: " << mCurrTextureUnitIndex << LL_ENDL;
    std::string active_enabled = "false";
    for (U32 i = 0; i < mTexUnits.size(); i++)
    {
        if (getTexUnit(i)->mCurrTexType != LLTexUnit::TT_NONE)
        {
            if (i == mCurrTextureUnitIndex) active_enabled = "true";
            LL_INFOS("TextureUnit") << "TexUnit: " << i << " Enabled" << LL_ENDL;
            LL_INFOS("TextureUnit") << "Enabled As: " ;
            switch (getTexUnit(i)->mCurrTexType)
            {
                case LLTexUnit::TT_TEXTURE:
                    LL_CONT << "Texture 2D";
                    break;
                case LLTexUnit::TT_RECT_TEXTURE:
                    LL_CONT << "Texture Rectangle";
                    break;
                case LLTexUnit::TT_CUBE_MAP:
                    LL_CONT << "Cube Map";
                    break;
                default:
                    LL_CONT << "ARGH!!! NONE!";
                    break;
            }
            LL_CONT << ", Texture Bound: " << getTexUnit(i)->mCurrTexture << LL_ENDL;
        }
    }
    LL_INFOS("TextureUnit") << "Active TexUnit Enabled : " << active_enabled << LL_ENDL;
}

glm::mat4 get_current_modelview()
{
    return glm::make_mat4(gGLModelView);
}

glm::mat4 get_current_projection()
{
    return glm::make_mat4(gGLProjection);
}

glm::mat4 get_last_modelview()
{
    return glm::make_mat4(gGLLastModelView);
}

glm::mat4 get_last_projection()
{
    return glm::make_mat4(gGLLastProjection);
}

void copy_matrix(const glm::mat4& src, F32* dst)
{
    auto matp = glm::value_ptr(src);
    for (U32 i = 0; i < 16; i++)
    {
        dst[i] = matp[i];
    }
}

void set_current_modelview(const glm::mat4& mat)
{
    copy_matrix(mat, gGLModelView);
}

void set_current_projection(const glm::mat4& mat)
{
    copy_matrix(mat, gGLProjection);
}

void set_last_modelview(const glm::mat4& mat)
{
    copy_matrix(mat, gGLLastModelView);
}

void set_last_projection(const glm::mat4& mat)
{
    copy_matrix(mat, gGLLastProjection);
}

glm::vec3 mul_mat4_vec3(const glm::mat4& mat, const glm::vec3& vec)
{
#if 1 // SIMD path results in strange crashes. Fall back to scalar for now.
    const float w = vec[0] * mat[0][3] + vec[1] * mat[1][3] + vec[2] * mat[2][3] + mat[3][3];
    return glm::vec3(
       (vec[0] * mat[0][0] + vec[1] * mat[1][0] + vec[2] * mat[2][0] + mat[3][0]) / w,
       (vec[0] * mat[0][1] + vec[1] * mat[1][1] + vec[2] * mat[2][1] + mat[3][1]) / w,
       (vec[0] * mat[0][2] + vec[1] * mat[1][2] + vec[2] * mat[2][2] + mat[3][2]) / w
    );
#else
    LLVector4a x, y, z, s, t, p, q;

    x.splat(vec.x);
    y.splat(vec.y);
    z.splat(vec.z);

    s.splat<3>(mat[0].data);
    t.splat<3>(mat[1].data);
    p.splat<3>(mat[2].data);
    q.splat<3>(mat[3].data);

    s.mul(x);
    t.mul(y);
    p.mul(z);
    q.add(s);
    t.add(p);
    q.add(t);

    x.mul(mat[0].data);
    y.mul(mat[1].data);
    z.mul(mat[2].data);

    x.add(y);
    z.add(mat[3].data);
    LLVector4a res;
    res.load3(glm::value_ptr(vec));
    res.setAdd(x, z);
    res.div(q);
    return glm::make_vec3(res.getF32ptr());
#endif
}
