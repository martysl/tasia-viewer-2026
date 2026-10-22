#!/usr/bin/env python3
"""Exercise production attachment bake refresh for SL magic IDs and real UUIDs."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def extract(path, signature):
    text = (ROOT / path).read_text()
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


fixture = r'''
#include <cassert>
#include <vector>
using U32 = unsigned;
struct LLUUID {
    int value = 0;
    bool operator==(const LLUUID& other) const { return value == other.value; }
};
struct LLViewerTexture {
    LLUUID id;
    const LLUUID& getID() const { return id; }
};
struct LLTextureEntry {
    LLUUID id;
    const LLUUID& getID() const { return id; }
};
namespace LLAvatarAppearanceDefines {
struct LLAvatarAppearanceDictionary {
    static bool isBakedImageId(const LLUUID& id) { return id.value >= 1 && id.value <= 10; }
};
}
struct LLVOAvatar {
    std::vector<LLViewerTexture*> bakes;
    U32 getNumBakes() const { return static_cast<U32>(bakes.size()); }
    LLViewerTexture* getBakedTexture(U32 i) { return bakes.at(i); }
};
struct LLViewerObject {
    LLVOAvatar* avatar = nullptr;
    std::vector<LLTextureEntry> entries;
    std::vector<LLViewerTexture*> updated;
    LLVOAvatar* getAvatar() { return avatar; }
    int getNumTEs() const { return static_cast<int>(entries.size()); }
    LLTextureEntry* getTE(int i) { return &entries.at(i); }
    LLViewerTexture* getBakedTextureForMagicId(const LLUUID& id) {
        return id.value == 1 ? avatar->bakes.at(0) : nullptr;
    }
    void changeTEImage(int face, LLViewerTexture* image) { updated.at(face) = image; }
    void refreshBakeTexture();
};
'''

main = r'''
int main() {
    LLViewerTexture baked0{{100}}, baked1{{200}};
    LLVOAvatar avatar{{&baked0, &baked1}};
    LLViewerObject object;
    object.avatar = &avatar;
    object.entries = {{{1}}, {{200}}, {{300}}};
    object.updated.resize(3, nullptr);
    object.refreshBakeTexture();
    assert(object.updated[0] == &baked0); // SL magic baked ID
    assert(object.updated[1] == &baked1); // real baked texture UUID
    assert(object.updated[2] == nullptr);  // unrelated texture remains untouched
}
'''

source = extract('llviewerobject.cpp', 'void LLViewerObject::refreshBakeTexture()')
with tempfile.TemporaryDirectory(prefix='tasia-bake-refresh-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(fixture + source + main)
    exe = Path(tmp) / 'test'
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

print('PASS: production bake refresh maps SL magic IDs and real bake UUIDs without touching unrelated textures.')
