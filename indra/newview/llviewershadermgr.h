/**
 * @file llviewershadermgr.h
 * @brief Viewer Shader Manager
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

#ifndef LL_VIEWER_SHADER_MGR_H
#define LL_VIEWER_SHADER_MGR_H

#include "llshadermgr.h"
#include "llmaterial.h"

#ifdef DX_RENDER
#include "llhlslshader.h"
// The whole viewer program set is built from one class per backend:
// LLHLSLShader under DX_RENDER, LLGLSLShader otherwise. Naming it once here
// keeps every program declaration below and every builder in
// llviewershadermgr.cpp backend-agnostic, instead of a #if around each of the
// ~200 uses. With DX_RENDER off this typedef expands to LLGLSLShader, so the
// GL build sees exactly what it saw before.
typedef LLHLSLShader LLViewerShaderProgram;
#else
typedef LLGLSLShader LLViewerShaderProgram;
#endif

#define LL_DEFERRED_MULTI_LIGHT_COUNT 16

class LLViewerShaderMgr: public LLShaderMgr
{
public:
    static bool sInitialized;
    static bool sSkipReload;

    LLViewerShaderMgr();
    /* virtual */ ~LLViewerShaderMgr();

    // Add shaders to mShaderList for later uniform propagation
    // Will assert on redundant shader entries in debug builds
    void finalizeShaderList();

    // singleton pattern implementation
    static LLViewerShaderMgr * instance();
    static void releaseInstance();

    void initAttribsAndUniforms(void);
    void setShaders();
    void unloadShaders();
    S32  getShaderLevel(S32 type);

    // loadBasicShaders in case of a failure returns
    // name of a file error happened at, otherwise
    // returns an empty string
    std::string loadBasicShaders();
    bool loadShadersEffects();
    bool loadShadersDeferred();
    bool loadShadersObject();
    bool loadShadersAvatar();
    bool loadShadersWater();
    bool loadShadersInterface();

    std::vector<S32> mShaderLevel;
    S32 mMaxAvatarShaderLevel;

    enum EShaderClass
    {
        SHADER_LIGHTING,
        SHADER_OBJECT,
        SHADER_AVATAR,
        SHADER_ENVIRONMENT,
        SHADER_INTERFACE,
        SHADER_EFFECT,
        SHADER_WINDLIGHT,
        SHADER_WATER,
        SHADER_DEFERRED,
        SHADER_COUNT
    };

    // simple model of forward iterator
    // http://www.sgi.com/tech/stl/ForwardIterator.html
    class shader_iter
    {
    private:
        friend bool operator == (shader_iter const & a, shader_iter const & b);
        friend bool operator != (shader_iter const & a, shader_iter const & b);

        typedef std::vector<LLViewerShaderProgram *>::const_iterator base_iter_t;
    public:
        shader_iter()
        {
        }

        shader_iter(base_iter_t iter) : mIter(iter)
        {
        }

        LLViewerShaderProgram & operator * () const
        {
            return **mIter;
        }

        LLViewerShaderProgram * operator -> () const
        {
            return *mIter;
        }

        shader_iter & operator++ ()
        {
            ++mIter;
            return *this;
        }

        shader_iter operator++ (int)
        {
            return mIter++;
        }

    private:
        base_iter_t mIter;
    };

    shader_iter beginShaders() const;
    shader_iter endShaders() const;

    /* virtual */ std::string getShaderDirPrefix(void);

    /* virtual */ void updateShaderUniforms(LLGLSLShader * shader);

#ifdef DX_RENDER
    /* virtual */ void updateShaderUniformsDX(LLHLSLShader * shader);
#endif

private:
    // the list of shaders we need to propagate parameters to.
    std::vector<LLViewerShaderProgram *> mShaderList;

}; //LLViewerShaderMgr

inline bool operator == (LLViewerShaderMgr::shader_iter const & a, LLViewerShaderMgr::shader_iter const & b)
{
    return a.mIter == b.mIter;
}

inline bool operator != (LLViewerShaderMgr::shader_iter const & a, LLViewerShaderMgr::shader_iter const & b)
{
    return a.mIter != b.mIter;
}

extern LLVector4            gShinyOrigin;

//utility shaders
extern LLViewerShaderProgram gOcclusionProgram;
extern LLViewerShaderProgram gOcclusionCubeProgram;
extern LLViewerShaderProgram gGlowCombineProgram;
extern LLViewerShaderProgram gReflectionMipProgram;
extern LLViewerShaderProgram gGaussianProgram;
extern LLViewerShaderProgram gRadianceGenProgram;
extern LLViewerShaderProgram gHeroRadianceGenProgram;
extern LLViewerShaderProgram gIrradianceGenProgram;
extern LLViewerShaderProgram gGlowCombineFXAAProgram;
extern LLViewerShaderProgram gDebugProgram;
enum NormalDebugShaderVariant : S32
{
    NORMAL_DEBUG_SHADER_DEFAULT,
    NORMAL_DEBUG_SHADER_WITH_TANGENTS,
    NORMAL_DEBUG_SHADER_COUNT
};
extern LLViewerShaderProgram gNormalDebugProgram[NORMAL_DEBUG_SHADER_COUNT];
extern LLViewerShaderProgram gSkinnedNormalDebugProgram[NORMAL_DEBUG_SHADER_COUNT];
extern LLViewerShaderProgram gClipProgram;
extern LLViewerShaderProgram gBenchmarkProgram;
extern LLViewerShaderProgram gReflectionProbeDisplayProgram;
extern LLViewerShaderProgram gCopyProgram;
extern LLViewerShaderProgram gCopyDepthProgram;
extern LLViewerShaderProgram gPBRTerrainBakeProgram;
extern LLViewerShaderProgram gDrawColorProgram;

//output tex0[tc0] - tex1[tc1]
extern LLViewerShaderProgram gTwoTextureCompareProgram;
//discard some fragments based on user-set color tolerance
extern LLViewerShaderProgram gOneTextureFilterProgram;


//object shaders
extern LLViewerShaderProgram gObjectPreviewProgram;
extern LLViewerShaderProgram gPhysicsPreviewProgram;
extern LLViewerShaderProgram gObjectBumpProgram;
extern LLViewerShaderProgram gSkinnedObjectBumpProgram;
extern LLViewerShaderProgram gObjectAlphaMaskNoColorProgram;

//environment shaders
extern LLViewerShaderProgram gWaterProgram;
extern LLViewerShaderProgram gUnderWaterProgram;
extern LLViewerShaderProgram gGlowProgram;
extern LLViewerShaderProgram gGlowExtractProgram;

//interface shaders
extern LLViewerShaderProgram gHighlightProgram;
extern LLViewerShaderProgram gHighlightNormalProgram;
extern LLViewerShaderProgram gHighlightSpecularProgram;

extern LLViewerShaderProgram gDeferredHighlightProgram;

extern LLViewerShaderProgram gPathfindingProgram;
extern LLViewerShaderProgram gPathfindingNoNormalsProgram;

// avatar shader handles
extern LLViewerShaderProgram gAvatarProgram;
extern LLViewerShaderProgram gAvatarEyeballProgram;
extern LLViewerShaderProgram gImpostorProgram;

// Post Process Shaders
extern LLViewerShaderProgram gPostScreenSpaceReflectionProgram;
extern LLViewerShaderProgram gPostVignetteProgram;   // <FS:CR> Import Vignette from Exodus
extern LLViewerShaderProgram gPostSnapshotFrameProgram;   // <FS:Beq/> Snapshot Frame overlay

// Deferred rendering shaders
extern LLViewerShaderProgram gDeferredImpostorProgram;
extern LLViewerShaderProgram gDeferredDiffuseProgram;
extern LLViewerShaderProgram gDeferredDiffuseAlphaMaskProgram;
extern LLViewerShaderProgram gDeferredNonIndexedDiffuseAlphaMaskProgram;
extern LLViewerShaderProgram gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram;
extern LLViewerShaderProgram gDeferredNonIndexedDiffuseProgram;
extern LLViewerShaderProgram gDeferredBumpProgram;
extern LLViewerShaderProgram gDeferredTerrainProgram;
extern LLViewerShaderProgram gDeferredTreeProgram;
extern LLViewerShaderProgram gDeferredTreeShadowProgram;
extern LLViewerShaderProgram gDeferredLightProgram;
extern LLViewerShaderProgram gDeferredMultiLightProgram[LL_DEFERRED_MULTI_LIGHT_COUNT];
extern LLViewerShaderProgram gDeferredSpotLightProgram;
extern LLViewerShaderProgram gDeferredMultiSpotLightProgram;
extern LLViewerShaderProgram gDeferredSunProgram;
extern LLViewerShaderProgram gDeferredSunProbeProgram;
extern LLViewerShaderProgram gHazeProgram;
extern LLViewerShaderProgram gHazeWaterProgram;
extern LLViewerShaderProgram gDeferredBlurLightProgram;
extern LLViewerShaderProgram gDeferredAvatarProgram;
extern LLViewerShaderProgram gDeferredSoftenProgram;
extern LLViewerShaderProgram gDeferredShadowProgram;
extern LLViewerShaderProgram gDeferredShadowCubeProgram;
extern LLViewerShaderProgram gDeferredShadowAlphaMaskProgram;
extern LLViewerShaderProgram gDeferredShadowGLTFAlphaMaskProgram;
extern LLViewerShaderProgram gDeferredShadowGLTFAlphaBlendProgram;
extern LLViewerShaderProgram gDeferredShadowFullbrightAlphaMaskProgram;
extern LLViewerShaderProgram gDeferredPostProgram;
extern LLViewerShaderProgram gDeferredCoFProgram;
extern LLViewerShaderProgram gDeferredDoFCombineProgram;
extern LLViewerShaderProgram gFXAAProgram[4];
extern LLViewerShaderProgram gSMAAEdgeDetectProgram[4];
extern LLViewerShaderProgram gSMAABlendWeightsProgram[4];
extern LLViewerShaderProgram gSMAANeighborhoodBlendProgram[4];
extern LLViewerShaderProgram gCASProgram;
extern LLViewerShaderProgram gCASLegacyGammaProgram;
extern LLViewerShaderProgram gDeferredPostNoDoFProgram;
extern LLViewerShaderProgram gDeferredPostNoDoFNoiseProgram;
extern LLViewerShaderProgram gDeferredPostGammaCorrectProgram;
extern LLViewerShaderProgram gLegacyPostGammaCorrectProgram;
extern LLViewerShaderProgram gDeferredPostTonemapProgram;
extern LLViewerShaderProgram gNoPostTonemapProgram;
extern LLViewerShaderProgram gDeferredPostTonemapGammaCorrectProgram;
extern LLViewerShaderProgram gNoPostTonemapGammaCorrectProgram;
extern LLViewerShaderProgram gDeferredPostTonemapLegacyGammaCorrectProgram;
extern LLViewerShaderProgram gNoPostTonemapLegacyGammaCorrectProgram;
extern LLViewerShaderProgram gExposureProgram;
extern LLViewerShaderProgram gExposureProgramNoFade;
extern LLViewerShaderProgram gLuminanceProgram;
extern LLViewerShaderProgram gDeferredAvatarShadowProgram;
extern LLViewerShaderProgram gDeferredAvatarAlphaShadowProgram;
extern LLViewerShaderProgram gDeferredAvatarAlphaMaskShadowProgram;
extern LLViewerShaderProgram gDeferredAlphaProgram;
extern LLViewerShaderProgram gHUDAlphaProgram;
extern LLViewerShaderProgram gDeferredAlphaImpostorProgram;
extern LLViewerShaderProgram gDeferredFullbrightProgram;
extern LLViewerShaderProgram gHUDFullbrightProgram;
extern LLViewerShaderProgram gDeferredFullbrightAlphaMaskProgram;
extern LLViewerShaderProgram gHUDFullbrightAlphaMaskProgram;
extern LLViewerShaderProgram gDeferredFullbrightAlphaMaskAlphaProgram;
extern LLViewerShaderProgram gHUDFullbrightAlphaMaskAlphaProgram;
extern LLViewerShaderProgram gDeferredEmissiveProgram;
extern LLViewerShaderProgram gDeferredAvatarEyesProgram;
extern LLViewerShaderProgram gDeferredAvatarAlphaProgram;
extern LLViewerShaderProgram gEnvironmentMapProgram;
extern LLViewerShaderProgram gDeferredWLSkyProgram;
extern LLViewerShaderProgram gDeferredWLCloudProgram;
extern LLViewerShaderProgram gDeferredWLSunProgram;
extern LLViewerShaderProgram gDeferredWLMoonProgram;
extern LLViewerShaderProgram gDeferredStarProgram;
extern LLViewerShaderProgram gDeferredFullbrightShinyProgram;
extern LLViewerShaderProgram gHUDFullbrightShinyProgram;
extern LLViewerShaderProgram gNormalMapGenProgram;
extern LLViewerShaderProgram gDeferredGenBrdfLutProgram;
extern LLViewerShaderProgram gDeferredBufferVisualProgram;
// [RLVa:KB] - @setsphere
extern LLViewerShaderProgram gRlvSphereProgram;
// [/RLVa:KB]

// Deferred materials shaders
extern LLViewerShaderProgram gDeferredMaterialProgram[LLMaterial::SHADER_COUNT*2];

extern LLViewerShaderProgram gHUDPBROpaqueProgram;
extern LLViewerShaderProgram gPBRGlowProgram;
extern LLViewerShaderProgram gDeferredPBROpaqueProgram;
extern LLViewerShaderProgram gDeferredPBRAlphaProgram;
extern LLViewerShaderProgram gHUDPBRAlphaProgram;

// GLTF shaders
extern LLViewerShaderProgram gGLTFPBRMetallicRoughnessProgram;

// Encodes detail level for dropping textures, in accordance with the GLTF spec where possible
// 0 is highest detail, -1 drops emissive, etc
// Dropping metallic roughness is off-spec - Reserve for potato machines as needed
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#additional-textures
enum TerrainPBRDetail : S32
{
    TERRAIN_PBR_DETAIL_MAX                = 0,
    TERRAIN_PBR_DETAIL_EMISSIVE           = 0,
    TERRAIN_PBR_DETAIL_OCCLUSION          = -1,
    TERRAIN_PBR_DETAIL_NORMAL             = -2,
    TERRAIN_PBR_DETAIL_METALLIC_ROUGHNESS = -3,
    TERRAIN_PBR_DETAIL_BASE_COLOR         = -4,
    TERRAIN_PBR_DETAIL_MIN                = -4,
};
enum TerrainPaintType : U32
{
    // Use LLVLComposition::mDatap (heightmap) generated by generateHeights, plus noise from TERRAIN_ALPHARAMP
    TERRAIN_PAINT_TYPE_HEIGHTMAP_WITH_NOISE = 0,
    // Use paint map if PBR terrain, otherwise fall back to TERRAIN_PAINT_TYPE_HEIGHTMAP_WITH_NOISE
    TERRAIN_PAINT_TYPE_PBR_PAINTMAP         = 1,
    TERRAIN_PAINT_TYPE_COUNT                = 2,
};
extern LLViewerShaderProgram gDeferredPBRTerrainProgram[TERRAIN_PAINT_TYPE_COUNT];
#endif
