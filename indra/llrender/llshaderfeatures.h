/**
 * @file llshaderfeatures.h
 * @brief Backend independent shader feature flags and uniform cache
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

// S24: LLShaderFeatures and LLShaderUniforms are plain data holders - a bag of
// feature flags and a list of pending uniform writes - with no GL, HLSL or
// D3D11 in them at all, so llglslshader.h and llhlslshader.h both need them.
// They used to be defined once in llglslshader.h and once (identically, minus
// the apply() parameter type) in llhlslshader.h, which is only legal because
// no single translation unit ever saw both headers - except llhlslshader.cpp,
// which pulls llshadermgr.h, which pulls llglslshader.h. One definition,
// included by both, is the fix; LLShaderMgr's feature-attaching code and
// LLGLSLShader's members then see the same types they always did.

#ifndef LL_LLSHADERFEATURES_H
#define LL_LLSHADERFEATURES_H

#include "llrender.h"

class LLGLSLShader;
#ifdef DX_RENDER
class LLHLSLShader;
#endif

class LLShaderFeatures
{
public:
    S32 mIndexedTextureChannels = 0;
    bool calculatesLighting = false;
    bool calculatesAtmospherics = false;
    bool hasLighting = false; // implies no transport (it's possible to have neither though)
    bool isAlphaLighting = false; // indicates lighting shaders need not be linked in (lighting performed directly in alpha shader to match deferred lighting functions)
    bool isSpecular = false;
    bool hasTransport = false; // implies no lighting (it's possible to have neither though)
    bool hasSkinning = false;
    bool hasObjectSkinning = false;
    bool mGLTF = false;
    bool hasAtmospherics = false;
    bool hasGamma = false;
    bool hasShadows = false;
    bool hasAmbientOcclusion = false;
    bool hasSrgb = false;
    bool isDeferred = false;
    bool hasFullGBuffer = false;
    bool hasScreenSpaceReflections = false;
    bool hasAlphaMask = false;
    bool hasReflectionProbes = false;
    bool attachNothing = false;
    bool hasHeroProbes = false;
    bool isPBRTerrain = false;
    bool hasTonemap = false;
};

// ============= Structure for caching shader uniforms ===============
class LLShaderUniforms
{
public:

    template<typename T>
    struct UniformSetting
    {
        S32 mUniform{ 0 };
        T mValue{};
    };

    typedef UniformSetting<S32> IntSetting;
    typedef UniformSetting<F32> FloatSetting;
    typedef UniformSetting<LLVector4> VectorSetting;
    typedef UniformSetting<LLVector3> Vector3Setting;

    void clear()
    {
        mIntegers.resize(0);
        mFloats.resize(0);
        mVectors.resize(0);
        mVector3s.resize(0);
    }

    void uniform1i(S32 index, S32 value)
    {
        mIntegers.push_back({ index, value });
    }

    void uniform1f(S32 index, F32 value)
    {
        mFloats.push_back({ index, value });
    }

    void uniform4fv(S32 index, const LLVector4& value)
    {
        mVectors.push_back({ index, value });
    }

    void uniform4fv(S32 index, const F32* value)
    {
        mVectors.push_back({ index, LLVector4(value) });
    }

    void uniform3fv(S32 index, const LLVector3& value)
    {
        mVector3s.push_back({ index, value });
    }

    void uniform3fv(S32 index, const F32* value)
    {
        mVector3s.push_back({ index, LLVector3(value) });
    }

    // S24: the LLGLSLShader form is defined in llglslshader.cpp, the
    // LLHLSLShader one in llhlslshader.cpp. Both files stay in the build under
    // DX_RENDER, so whichever overload a caller needs is always defined.
    void apply(LLGLSLShader* shader);
#ifdef DX_RENDER
    void apply(LLHLSLShader* shader);
#endif


    std::vector<IntSetting> mIntegers;
    std::vector<FloatSetting> mFloats;
    std::vector<VectorSetting> mVectors;
    std::vector<Vector3Setting> mVector3s;
};

#endif // LL_LLSHADERFEATURES_H
