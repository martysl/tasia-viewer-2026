/**
 * @file llbakereadback.h
 * @brief Bounded pixel conversion for local avatar bakes and DX11 readback.
 *
 * Copyright (C) 2026 Tasia. LGPL-2.1-or-later.
 */
#ifndef LL_LLBAKEREADBACK_H
#define LL_LLBAKEREADBACK_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace LLBake
{
// Local bakes are bounded independently of arbitrary asset/terrain sizes.
constexpr int MAX_DIMENSION = 2048;

inline bool pixelCount(int width, int height, std::size_t& count)
{
    count = 0;
    if (width <= 0 || height <= 0 || width > MAX_DIMENSION || height > MAX_DIMENSION)
    {
        return false;
    }
    count = static_cast<std::size_t>(width) * height;
    return true;
}

inline bool copyAlpha(const std::uint8_t* rgba, std::size_t rgba_size,
                      std::uint8_t* alpha, std::size_t alpha_size,
                      int width, int height)
{
    std::size_t pixels = 0;
    if (!pixelCount(width, height, pixels) || !rgba || !alpha ||
        rgba_size < pixels * 4 || alpha_size < pixels)
    {
        return false;
    }
    for (std::size_t i = 0; i < pixels; ++i)
    {
        alpha[i] = rgba[i * 4 + 3];
    }
    return true;
}

struct PixelSummary
{
    std::size_t pixels = 0;
    std::size_t non_black = 0;
    std::size_t nontransparent_black = 0;
    std::size_t transparent = 0;
};

inline bool isAllBlackNonTransparent(const PixelSummary& summary)
{
    return summary.pixels > 0 && summary.non_black == 0 &&
           summary.nontransparent_black == summary.pixels && summary.transparent == 0;
}

// Aggregate-only diagnostics: never expose avatar pixels, IDs, or texture data.
inline bool summarizeRGBA(const std::uint8_t* rgba, std::size_t rgba_size,
                          int width, int height, PixelSummary& summary)
{
    std::size_t pixels = 0;
    summary = {};
    if (!pixelCount(width, height, pixels) || !rgba || rgba_size < pixels * 4)
    {
        return false;
    }
    summary.pixels = pixels;
    for (std::size_t i = 0; i < pixels; ++i)
    {
        const std::uint8_t* pixel = rgba + i * 4;
        const bool black = pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0;
        if (!black) ++summary.non_black;
        else if (pixel[3] != 0) ++summary.nontransparent_black;
        if (pixel[3] == 0) ++summary.transparent;
    }
    return true;
}

// Preserve RGBA (including eyelash/transparency alpha) and append the clothing
// morph mask as channel five, matching the reference's RGBHM J2C payload.
inline bool packFiveChannels(const std::uint8_t* rgba, std::size_t rgba_size,
                             const std::uint8_t* mask, std::size_t mask_size,
                             std::uint8_t* output, std::size_t output_size,
                             int width, int height)
{
    std::size_t pixels = 0;
    if (!pixelCount(width, height, pixels) || !rgba || !mask || !output ||
        rgba_size < pixels * 4 || mask_size < pixels || output_size < pixels * 5)
    {
        return false;
    }
    for (std::size_t i = 0; i < pixels; ++i)
    {
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            output[i * 5 + channel] = rgba[i * 4 + channel];
        }
        output[i * 5 + 4] = mask[i];
    }
    return true;
}
}

#ifdef DX_RENDER
#include "llrendertarget.h"
#include "DXReadback.h"

namespace LLBake
{
inline bool readColor(LLRenderTarget* target, int x, int y, int width, int height,
                      std::vector<std::uint8_t>& rgba)
{
    rgba.clear();
    std::size_t pixels = 0;
    if (!target || !pixelCount(width, height, pixels) || x < 0 || y < 0)
    {
        return false;
    }
    ID3D11Texture2D* texture = target->getDXColorTexture(0);
    if (!texture)
    {
        return false;
    }
    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);
    // DXReadback copies bytes, not pixels of an arbitrary format. In particular,
    // never read compressed/float/MSAA resources as four-byte RGBA.
    const bool bgra = desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                      desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    const bool color = desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
                       desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || bgra;
    if (!color || desc.SampleDesc.Count != 1 ||
        static_cast<unsigned int>(x) > desc.Width ||
        static_cast<unsigned int>(y) > desc.Height ||
        static_cast<unsigned int>(width) > desc.Width - static_cast<unsigned int>(x) ||
        static_cast<unsigned int>(height) > desc.Height - static_cast<unsigned int>(y))
    {
        return false;
    }
    rgba.resize(pixels * 4);
    // Same top-left, tightly packed CPU convention as LLImageDX::readBackRaw().
    // DXReadback removes D3D RowPitch padding; do not flip only one of color and
    // morph-mask data, or the five J2C channels will describe different pixels.
    if (!DXReadback::readPixels(texture, x, y, width, height, 4, rgba.data()))
    {
        rgba.clear();
        return false;
    }
    if (bgra)
    {
        for (std::size_t i = 0; i < pixels; ++i)
        {
            const std::uint8_t red = rgba[i * 4 + 2];
            rgba[i * 4 + 2] = rgba[i * 4];
            rgba[i * 4] = red;
        }
    }
    return true;
}
}
#endif
#endif
