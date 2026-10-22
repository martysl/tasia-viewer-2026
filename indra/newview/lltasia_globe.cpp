#include "llviewerprecompiledheaders.h"
#include "lltasia_globe.h"
#include "lltasia_globe_math.h"
#include "llworldmap.h"
#include "llviewertexturelist.h"
#include "llfocusmgr.h"
#include "llrender.h"
#include "llui.h"
#include "llfontdx.h"
#include "lllocalcliprect.h"
#include <limits>

LLTasiaGlobePanel::LLTasiaGlobePanel(std::function<void(const LLVector3d&)> select)
: mSelect(std::move(select))
{
    setName("tasia_globe");
    setMouseOpaque(true);
    setFollowsAll();
    mPlanet = LLViewerTextureManager::getFetchedTextureFromFile(
        "world/tasia_planet.png", FTT_LOCAL_FILE, true, LLDXTexture::BOOST_UI);
}

void LLTasiaGlobePanel::draw()
{
    using namespace TasiaGlobe;
    const F32 width = getRect().getWidth(), height = getRect().getHeight();
    const F32 cx = width * .5f, cy = height * .5f;
    const F32 radius = llmin(width, height) * .40f * static_cast<F32>(mZoom);
    LLLocalClipRect clip(getLocalRect());
    gDX.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    gDX.color4f(.008f, .015f, .035f, 1.f);
    dx_rect_2d(0, height, width, 0);
    // Deterministic star field and atmosphere. No request leaves this viewer.
    for (int i = 0; i < 100; ++i)
    {
        const S32 x = (i * 7919 + 17) % llmax(1, static_cast<S32>(width));
        const S32 y = (i * 3571 + 53) % llmax(1, static_cast<S32>(height));
        gDX.color4f(.28f, .43f, .59f, .65f);
        dx_rect_2d(x, y + 1, x + 1, y);
    }
    for (int ring = 7; ring > 0; --ring)
    {
        const F32 outer = radius + ring * 1.7f, inner = outer - 1.8f;
        gDX.color4f(.07f, .43f, .86f, .045f * (8 - ring));
        gDX.begin(LLRender::TRIANGLES);
        for (int s = 0; s < 128; ++s)
        {
            const double a = 2 * pi * s / 128, b = 2 * pi * (s + 1) / 128;
            auto vertex = [&](double angle, F32 r) { gDX.vertex2f(cx + r * std::cos(angle), cy + r * std::sin(angle)); };
            vertex(a, inner); vertex(a, outer); vertex(b, outer);
            vertex(a, inner); vertex(b, outer); vertex(b, inner);
        }
        gDX.end();
    }
    if (mPlanet.notNull())
    {
        mPlanet->setKnownDrawSize(1024, 512);
        mPlanet->addTextureStats(1024.f * 512.f);
        gDX.getTexUnit(0)->bind(mPlanet);
    }
    // Orthographic projection of a real rotating sphere, shaded by its 3D normal.
    // Only the front hemisphere is rendered/pickable. Texture is original art,
    // not a claim that virtual grid coordinates are real terrestrial locations.
    constexpr int longitude_steps = 96, latitude_steps = 48;
    auto vertex = [&](int x, int y)
    {
        const double u = double(x) / longitude_steps, v = double(y) / latitude_steps;
        return Vertex{project((u - .5) * 2 * pi, (v - .5) * pi, mYaw, mPitch), u, v};
    };
    auto triangle = [&](Vertex a, Vertex b, Vertex c)
    {
        const auto polygon = frontTriangle(a,b,c);
        for (int i = 1; i+1 < polygon.count; ++i)
        for (const auto& v : {polygon.points[0], polygon.points[i], polygon.points[i+1]})
        {
            const F32 light = static_cast<F32>(std::clamp(.22 + .78 *
                (v.p.x * -.35 + v.p.y * .45 + v.p.z * .82), .12, 1.0));
            gDX.color4f(light, light, light, 1);
            gDX.texCoord2f(v.u, v.v);
            gDX.vertex2f(cx + radius * v.p.x, cy + radius * v.p.y);
        }
    };
    gDX.begin(LLRender::TRIANGLES);
    for (int y = 0; y < latitude_steps; ++y)
        for (int x = 0; x < longitude_steps; ++x)
        {
            const auto a = vertex(x,y), b = vertex(x+1,y), c = vertex(x+1,y+1), d = vertex(x,y+1);
            triangle(a,b,c); triangle(a,c,d);
        }
    gDX.end();
    gDX.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    // The atlas places known regions by their relative grid coordinates. It does
    // not invent destinations; each marker retains its original global position.
    const auto& regions = LLWorldMap::instance().getRegionMap();
    double minx = (std::numeric_limits<double>::max)(), miny = minx;
    double maxx = -minx, maxy = -minx;
    for (const auto& entry : regions)
    {
        const auto origin = entry.second->getGlobalOrigin();
        minx = std::min(minx, origin.mdV[0]); maxx = std::max(maxx, origin.mdV[0]);
        miny = std::min(miny, origin.mdV[1]); maxy = std::max(maxy, origin.mdV[1]);
    }
    mMarkers.clear();
    for (const auto& entry : regions)
    {
        if (mMarkers.size() >= 2048) break;
        const auto origin = entry.second->getGlobalOrigin();
        const auto p = project(atlas(origin.mdV[0], minx, maxx, pi * .82),
            atlas(origin.mdV[1], miny, maxy, pi * .36), mYaw, mPitch);
        if (p.z <= .02 || entry.second->getName().empty()) continue;
        Marker marker{static_cast<F32>(cx + radius*p.x), static_cast<F32>(cy + radius*p.y),
            origin + LLVector3d(128,128,25), entry.second->getName(), entry.second->isDown()};
        mMarkers.push_back(marker);
        gDX.color4f(marker.down ? .95f : .25f, marker.down ? .25f : 1.f, .58f, 1.f);
        dx_rect_2d(marker.x-3, marker.y+3, marker.x+3, marker.y-3);
        if (mMarkers.size() <= 24)
            LLFontDX::getFontSansSerifSmall()->renderUTF8(marker.label, 0, marker.x+6, marker.y,
                LLColor4(.85f,.96f,1.f,1.f), LLFontDX::LEFT, LLFontDX::BASELINE, LLFontDX::NORMAL,
                LLFontDX::DROP_SHADOW, S32_MAX, 180);
    }
}

bool LLTasiaGlobePanel::handleMouseDown(S32 x, S32 y, MASK)
{
    mDownX = mLastX = x; mDownY = mLastY = y; mDragged = false;
    gFocusMgr.setMouseCapture(this);
    return true;
}

bool LLTasiaGlobePanel::handleHover(S32 x, S32 y, MASK)
{
    if (hasMouseCapture())
    {
        mDragged = mDragged || std::abs(x-mDownX) + std::abs(y-mDownY) > 4;
        mYaw += (x-mLastX) * .008;
        mPitch = std::clamp(mPitch + (y-mLastY) * .008, -1.3, 1.3);
        mLastX = x; mLastY = y;
    }
    return true;
}

bool LLTasiaGlobePanel::handleMouseUp(S32 x, S32 y, MASK)
{
    if (!hasMouseCapture()) return false;
    gFocusMgr.setMouseCapture(nullptr);
    if (!mDragged)
    {
        const Marker* closest = nullptr;
        F32 distance = 225;
        for (const auto& marker : mMarkers)
        {
            const F32 d = (marker.x-x)*(marker.x-x)+(marker.y-y)*(marker.y-y);
            if (!marker.down && d < distance) { closest = &marker; distance = d; }
        }
        if (closest && mSelect) mSelect(closest->destination); // selection only, never auto-teleport
    }
    return true;
}

bool LLTasiaGlobePanel::handleScrollWheel(S32, S32, S32 clicks)
{
    mZoom = TasiaGlobe::zoom(mZoom, clicks);
    return true;
}
