#pragma once
#include "llpanel.h"
#include "llviewertexture.h"
#include "v3dmath.h"
#include <functional>

// Native DX-rendered globe, no embedded webpage, external tiles or API key.
class LLTasiaGlobePanel final : public LLPanel
{
public:
    explicit LLTasiaGlobePanel(std::function<void(const LLVector3d&)> select);
    void draw() override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleMouseUp(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;
private:
    struct Marker { F32 x, y; LLVector3d destination; std::string label; bool down; };
    std::vector<Marker> mMarkers;
    LLPointer<LLViewerFetchedTexture> mPlanet;
    std::function<void(const LLVector3d&)> mSelect;
    double mYaw = 0, mPitch = 0, mZoom = 1;
    S32 mLastX = 0, mLastY = 0, mDownX = 0, mDownY = 0;
    bool mDragged = false;
};
