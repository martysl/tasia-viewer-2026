/** @file lltasia_legacy_wearables.h
 * Bounded validation of an OpenSim initial worn-item snapshot.
 * Copyright (C) 2026 Tasia. LGPL-2.1-or-later.
 */
#ifndef LL_LLTASIA_LEGACY_WEARABLES_H
#define LL_LLTASIA_LEGACY_WEARABLES_H
#include <array>
#include <cstddef>
#include <vector>

namespace TasiaWearables
{
// Structural diagnostics only. Never expose UUIDs/asset IDs or change acceptance
// policy merely because a failed initial outfit left the avatar as a cloud.
struct SnapshotDiagnostics
{
    std::size_t entries = 0, null_items = 0, null_assets = 0, invalid_types = 0;
    std::size_t duplicate_items = 0, shared_assets = 0;
    unsigned null_body_item_mask = 0, null_body_asset_mask = 0;
    std::array<std::size_t, 4> body{};
};

template<class Entry>
SnapshotDiagnostics diagnoseSnapshot(const std::vector<Entry>& entries, std::size_t type_count)
{
    SnapshotDiagnostics result;
    result.entries = entries.size();
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        result.null_items += entry.item.isNull();
        result.null_assets += entry.asset.isNull();
        result.invalid_types += static_cast<std::size_t>(entry.type) >= type_count;
        if (entry.type < 4)
        {
            ++result.body[entry.type];
            if (entry.item.isNull()) result.null_body_item_mask |= 1u << entry.type;
            if (entry.asset.isNull()) result.null_body_asset_mask |= 1u << entry.type;
        }
        for (std::size_t j = 0; j < i; ++j)
        {
            result.duplicate_items += !entry.item.isNull() && entries[j].item == entry.item;
            result.shared_assets += !entry.asset.isNull() && entries[j].asset == entry.asset;
        }
    }
    return result;
}

// A partial repair must contain exactly the requested real body parts. When
// COF targets could not be fetched, require an exact item-ID match for each.
template<class Entry, class ID>
bool validBodyPartRepair(const std::vector<Entry>& entries, unsigned mask,
                         const std::vector<ID>& unresolved_targets)
{
    if (!mask || (mask & ~15u) || entries.empty() || entries.size() > 4) return false;
    unsigned seen = 0;
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        if (entry.type >= 4 || entry.item.isNull() || entry.asset.isNull()) return false;
        const unsigned bit = 1u << entry.type;
        if (!(mask & bit) || (seen & bit)) return false;
        seen |= bit;
        bool matched = unresolved_targets.empty();
        for (const auto& target : unresolved_targets) if (target == entry.item) matched = true;
        if (!matched) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (entries[j].item == entry.item || entries[j].asset == entry.asset) return false;
    }
    return seen == mask;
}

// Entry supplies type, item and asset; IDs supply isNull() and equality.
// The four original body-part types are shape, skin, hair and eyes (0..3).
template<class Entry>
bool validSnapshot(const std::vector<Entry>& entries, std::size_t type_count,
                   std::size_t max_clothing)
{
    if (type_count < 4 || type_count > 64 || max_clothing > 60 ||
        entries.size() < 4 || entries.size() > 4 + max_clothing)
    {
        return false;
    }
    std::array<unsigned, 4> body{};
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        if (static_cast<std::size_t>(entry.type) >= type_count ||
            entry.item.isNull() || entry.asset.isNull()) return false;
        if (entry.type < 4 && ++body[entry.type] != 1) return false;
        for (std::size_t j = 0; j < i; ++j)
        {
            // LLWearableList caches one mutable wearable per asset. Until it
            // supports per-item instances, don't misassign a shared asset to
            // two different worn inventory items or collapse either layer.
            if (entries[j].item == entry.item || entries[j].asset == entry.asset) return false;
        }
    }
    for (unsigned count : body) if (count != 1) return false;
    return true;
}
}
#endif
