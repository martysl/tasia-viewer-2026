/**
 * @file lltasia_region_compat.h
 * @brief Bounded OpenSim region-size and parcel wire-format policy.
 * Copyright (C) 2026, Tasia Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef LL_TASIA_REGION_COMPAT_H
#define LL_TASIA_REGION_COMPAT_H

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>

namespace LLTasiaRegion
{
constexpr std::uint32_t DEFAULT_WIDTH = 256;
// Checked OpenSim Constants.MaximumRegionSize is 4096m. Keep terrain, map,
// and parcel allocation guards aligned with that server-side ceiling.
constexpr std::uint32_t MAX_WIDTH = 4096;
constexpr std::uint32_t MAP_SIZE_STEP = 16;
constexpr std::uint32_t PARCEL_STEP = 4;
constexpr std::size_t OVERLAY_CHUNK_BYTES = 1024;

// The existing terrain/composition renderer is square and power-of-two based.
// Do not silently represent a rectangular or unsupported region as its X size.
constexpr bool supportedDimensions(std::uint32_t x, std::uint32_t y)
{
    return x == y && x >= DEFAULT_WIDTH && x <= MAX_WIDTH && (x & (x - 1)) == 0;
}

constexpr std::uint32_t regionWidth(bool opensim, std::uint32_t x, std::uint32_t y)
{
    return opensim && supportedDimensions(x, y) ? x : DEFAULT_WIDTH;
}

// Geometric adjacency includes shared edges and corners but rejects both
// separated and area-overlapping regions. Region origins/sizes come from
// integer world coordinates, so exact boundary comparisons are intentional.
constexpr bool regionsAdjacent(double ax, double ay, double aw,
                               double bx, double by, double bw)
{
    if (!(aw > 0.0) || !(bw > 0.0))
        return false;

    const double overlap_x = std::min(ax + aw, bx + bw) - std::max(ax, bx);
    const double overlap_y = std::min(ay + aw, by + bw) - std::max(ay, by);
    return overlap_x >= 0.0 && overlap_y >= 0.0 &&
           !(overlap_x > 0.0 && overlap_y > 0.0);
}

// Return aligned patch-index spans for the shared part of two region edges.
// The caller supplies each edge's coordinate interval; the result is suitable
// for one-to-one patch pairing when both regions use the same world patch size.
inline bool sharedPatchSpan(double a0, double a1, double b0, double b1,
                            double patch_size, std::uint32_t& a_first,
                            std::uint32_t& b_first, std::uint32_t& patch_count)
{
    if (!(patch_size > 0.0) || !(a1 > a0) || !(b1 > b0))
        return false;

    const double start = std::max(a0, b0);
    const double end = std::min(a1, b1);
    if (!(end > start))
        return false;

    const double a_offset = start - a0;
    const double b_offset = start - b0;
    const double span = end - start;
    const double a_steps = a_offset / patch_size;
    const double b_steps = b_offset / patch_size;
    const double count_steps = span / patch_size;
    if (std::floor(a_steps) != a_steps || std::floor(b_steps) != b_steps ||
        std::floor(count_steps) != count_steps ||
        a_steps > std::numeric_limits<std::uint32_t>::max() ||
        b_steps > std::numeric_limits<std::uint32_t>::max() ||
        count_steps > std::numeric_limits<std::uint32_t>::max())
        return false;

    a_first = static_cast<std::uint32_t>(a_steps);
    b_first = static_cast<std::uint32_t>(b_steps);
    patch_count = static_cast<std::uint32_t>(count_steps);
    return patch_count > 0;
}

// Map extents are independent of terrain renderer support: preserve valid
// rectangular sizes while bounding malformed protocol values.
constexpr bool validMapDimension(std::uint32_t size)
{
    return size >= MAP_SIZE_STEP && size <= MAX_WIDTH && size % MAP_SIZE_STEP == 0;
}

constexpr bool validMapDimensions(std::uint32_t size_x, std::uint32_t size_y)
{
    return validMapDimension(size_x) && validMapDimension(size_y);
}

constexpr std::uint32_t safeMapDimension(std::uint32_t size)
{
    return validMapDimension(size) ? size : DEFAULT_WIDTH;
}

// OpenSim may advertise a negative simulator floor. Keep SL at its historical
// zero floor and ignore malformed feature values rather than propagating NaN.
inline float safeMinSimHeight(bool opensim, float advertised_min)
{
    return opensim && std::isfinite(advertised_min) && advertised_min < 0.f
        ? advertised_min : 0.f;
}

inline float clampWorldMapTeleportZ(bool opensim, float min_sim_height, float z)
{
    const float min_z = safeMinSimHeight(opensim, min_sim_height);
    return std::isfinite(z) ? std::clamp(z, min_z, 4096.f) : min_z;
}

// ChatModule advertises an integer say distance; reject malformed or
// non-representable values so a bad feature cannot turn a local list global.
inline float safeSayRange(bool opensim, double advertised_range)
{
    return opensim && std::isfinite(advertised_range) && advertised_range >= 0.0 &&
        advertised_range <= static_cast<double>(std::numeric_limits<std::int32_t>::max())
        ? static_cast<float>(advertised_range) : 20.f;
}

// Validate before multiplication: negative/huge sequence IDs cannot overflow an
// offset or address outside the actual ownership allocation. The protocol's
// chunk size remains 1024 bytes on both SL and OpenSim, regardless of region size.
constexpr bool overlayChunkFits(std::size_t allocation, std::int32_t sequence,
                                std::size_t bytes)
{
    return sequence >= 0 && bytes == OVERLAY_CHUNK_BYTES &&
           allocation >= bytes && allocation % bytes == 0 &&
           static_cast<std::size_t>(sequence) < allocation / bytes;
}
}
#endif
