#!/usr/bin/env python3
"""Compile and exercise the production local-texture readiness method."""
import subprocess
import tempfile
from pathlib import Path

indra = Path(__file__).resolve().parents[2]
source = (indra / 'newview/llvoavatarself.cpp').read_text()
signature = 'S32 LLVOAvatarSelf::getLocalDiscardLevel(ETextureIndex type, U32 wearable_index) const'
start = source.index(signature)
opening = source.index('{', start)
end, depth = opening + 1, 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
production_method = source[start:end]


def production_function(signature):
    start = source.index(signature)
    opening = source.index('{', start)
    end, depth = opening + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


production_available = production_function(
    'bool LLVOAvatarSelf::isLocalTextureDataAvailable(const LLViewerTexLayerSet* layerset) const')
production_final = production_function(
    'bool LLVOAvatarSelf::isLocalTextureDataFinal(const LLViewerTexLayerSet* layerset) const')

fixture = r'''
#include <cassert>
#include <cstdint>
#include <map>
#include <vector>
using S32 = int;
using U32 = std::uint32_t;
using ETextureIndex = int;
using EBakedTextureIndex = int;
constexpr int IMG_DEFAULT_AVATAR = 0;
namespace LLWearableType { enum EType { WT_SKIN = 0 }; }
struct LLDXTexture { virtual ~LLDXTexture() = default; };
struct LLViewerFetchedTexture : LLDXTexture {
    bool missing = false;
    S32 discard = 0;
    bool isMissingAsset() const { return missing; }
    S32 getDiscardLevel() const { return discard; }
};
struct LLLocalTextureObject {
    int id = 1;
    LLDXTexture* image = nullptr;
    int getID() const { return id; }
    LLDXTexture* getImage() const { return image; }
};
struct LLViewerTexLayerSet {};
using texture_vec_t = std::vector<ETextureIndex>;
namespace LLAvatarAppearanceDictionary {
struct BakedEntry { texture_vec_t mLocalTextures; };
struct BakedTextures : std::map<EBakedTextureIndex, BakedEntry*> {};
}
struct AvatarDictionary {
    LLAvatarAppearanceDictionary::BakedTextures baked;
    const LLAvatarAppearanceDictionary::BakedTextures& getBakedTextures() const { return baked; }
    const LLAvatarAppearanceDictionary::BakedEntry* getBakedTexture(EBakedTextureIndex i) const {
        return baked.at(i);
    }
    LLWearableType::EType getTEWearableType(ETextureIndex) const { return LLWearableType::WT_SKIN; }
};
struct BakedData { const LLViewerTexLayerSet* mTexLayerSet = nullptr; };
struct Wearables { U32 getWearableCount(LLWearableType::EType) const { return 1; } } gAgentWearables;
struct Settings { U32 getU32(const char*) const { return 0; } } gSavedSettings;
#define llassert(condition) assert(condition)
struct LLVOAvatarSelf {
    bool local = true;
    const LLLocalTextureObject* texture = nullptr;
    static AvatarDictionary* sAvatarDictionary;
    std::vector<BakedData> mBakedTextureDatas = std::vector<BakedData>(1);
    bool isIndexLocalTexture(ETextureIndex) const { return local; }
    const LLLocalTextureObject* getLocalTextureObject(ETextureIndex, U32) const { return texture; }
    S32 getLocalDiscardLevel(ETextureIndex type, U32 wearable_index) const;
    bool isLocalTextureDataAvailable(const LLViewerTexLayerSet* layerset) const;
    bool isLocalTextureDataFinal(const LLViewerTexLayerSet* layerset) const;
};
AvatarDictionary* LLVOAvatarSelf::sAvatarDictionary = nullptr;
'''

tests = r'''
int main() {
    LLVOAvatarSelf avatar;
    AvatarDictionary dictionary;
    LLAvatarAppearanceDictionary::BakedEntry baked_entry;
    baked_entry.mLocalTextures.push_back(1);
    dictionary.baked.emplace(0, &baked_entry);
    LLVOAvatarSelf::sAvatarDictionary = &dictionary;
    LLViewerTexLayerSet layerset;
    avatar.mBakedTextureDatas[0].mTexLayerSet = &layerset;
    LLViewerFetchedTexture image;
    LLLocalTextureObject object;
    object.image = &image;
    avatar.texture = &object;

    image.discard = 2;
    assert(avatar.getLocalDiscardLevel(1, 0) == 2);
    assert(avatar.isLocalTextureDataAvailable(&layerset));
    assert(!avatar.isLocalTextureDataFinal(&layerset));
    image.discard = -1;
    assert(avatar.getLocalDiscardLevel(1, 0) == -1);
    assert(!avatar.isLocalTextureDataAvailable(&layerset));
    assert(!avatar.isLocalTextureDataFinal(&layerset));
    image.discard = 0;
    image.missing = true;
    assert(avatar.getLocalDiscardLevel(1, 0) == -1);
    assert(!avatar.isLocalTextureDataAvailable(&layerset));
    assert(!avatar.isLocalTextureDataFinal(&layerset));
    image.missing = false;
    object.image = nullptr;
    assert(avatar.getLocalDiscardLevel(1, 0) == -1);
    assert(!avatar.isLocalTextureDataAvailable(&layerset));
    assert(!avatar.isLocalTextureDataFinal(&layerset));

    object.id = IMG_DEFAULT_AVATAR;
    assert(avatar.getLocalDiscardLevel(1, 0) == 0);
    assert(avatar.isLocalTextureDataAvailable(&layerset));
    assert(avatar.isLocalTextureDataFinal(&layerset));
    avatar.texture = nullptr;
    object.id = 1;
    assert(avatar.getLocalDiscardLevel(1, 0) == 0); // absent optional texture
    avatar.local = false;
    assert(avatar.getLocalDiscardLevel(1, 0) == 0);
}
'''

with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / 'readiness.cpp'
    exe = Path(tmp) / 'readiness'
    cpp.write_text(fixture + '\n' + production_method + '\n' + production_available +
                   '\n' + production_final + '\n' + tests)
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

print('PASS: missing/undecoded local assets block both available/final readiness; default/absent optional textures retain existing behavior')
