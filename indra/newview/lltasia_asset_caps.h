/** Preserve opaque OpenSim capability paths; retain the SL request convention. */
#ifndef LL_TASIA_ASSET_CAPS_H
#define LL_TASIA_ASSET_CAPS_H
#include <string>
namespace TasiaAssetCaps
{
inline std::string mesh(const std::string& viewer_asset, const std::string& mesh2,
                        const std::string& mesh1, bool opensim)
{
    if (!viewer_asset.empty() || !opensim) return viewer_asset;
    return !mesh2.empty() ? mesh2 : mesh1;
}

inline std::string assetURL(const std::string& cap, const std::string& type,
                            const std::string& id, bool opensim)
{
    if (cap.empty()) return {};
    if (!opensim) return cap + "/?" + type + "_id=" + id;
    // Capabilities are opaque endpoints: adding a path slash changes their
    // routing on strict reverse proxies. Only add the requested query item.
    const bool has_query = cap.find('?') != std::string::npos;
    const std::string separator = !has_query ? "?" :
        (cap.back() == '?' || cap.back() == '&' ? "" : "&");
    return cap + separator + type + "_id=" + id;
}
}
#endif
