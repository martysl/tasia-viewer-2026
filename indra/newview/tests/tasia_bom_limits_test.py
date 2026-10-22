#!/usr/bin/env python3
"""Compile production BOM limits/pending-upload methods against synthetic regions."""
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]


def extract(path, signature):
    text = (root / path).read_text()
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


fixture = r'''
#include <cassert>
using S32=int;
enum { BAKED_HEAD, BAKED_UPPER, BAKED_LOWER, BAKED_EYES, BAKED_SKIRT,
       BAKED_HAIR, BAKED_LEFT_ARM, BAKED_LEFT_LEG, BAKED_AUX1,
       BAKED_AUX2, BAKED_AUX3, BAKED_NUM_INDICES };
struct LLGridManager {
 bool sl=false;
 static LLGridManager* getInstance() { static LLGridManager g; return &g; }
 bool isInSecondLife() const { return sl; }
};
struct Region { bool bom=false; bool bakesOnMeshEnabled() const { return bom; } } region;
struct Buffer { bool pending=false; bool uploadPending() const { return pending; } } buffers[11];
struct LLViewerTexLayerSet {
 Buffer* buffer=nullptr;
 Buffer* getViewerComposite() const { return buffer; }
} layers[11];
struct LLVOAvatar {
 Region* region_ptr=&region;
 Region* getRegion() const { return region_ptr; }
 S32 getNumBakes() const;
};
struct LLVOAvatarSelf: LLVOAvatar {
 const LLViewerTexLayerSet* getTexLayerSet(int i) const {
  assert(i>=0 && i<11); return &layers[i];
 }
 bool hasPendingBakedUploads() const;
};
'''
main = r'''
int main() {
 LLVOAvatarSelf av;
 for(int i=0;i<11;++i) layers[i].buffer=&buffers[i];
 assert(av.getNumBakes()==6);
 buffers[10].pending=true;
 assert(!av.hasPendingBakedUploads());
 region.bom=true;
 assert(av.getNumBakes()==11);
 assert(av.hasPendingBakedUploads());
 buffers[10].pending=false;
 assert(!av.hasPendingBakedUploads());
 av.region_ptr=nullptr;
 assert(av.getNumBakes()==6);
 LLGridManager::getInstance()->sl=true;
 assert(av.getNumBakes()==11);
 region.bom=false; av.region_ptr=&region;
 assert(av.getNumBakes()==11);
}
'''
header = (root / 'llvoavatar.h').read_text()
declaration = header.split('bool            lineSegmentIntersect(', 1)[1].split(';', 1)[0]
for parameter in ['S32 face = -1', 'bool pick_transparent = false',
                  'bool pick_rigged = false', 'bool pick_unselectable = true',
                  'S32* face_hit = NULL']:
    assert parameter in declaration, parameter
with tempfile.TemporaryDirectory(prefix='tasia-bom-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(fixture + extract('llvoavatar.cpp', 'S32 LLVOAvatar::getNumBakes() const') +
                   extract('llvoavatarself.cpp', 'bool LLVOAvatarSelf::hasPendingBakedUploads() const') + main)
    exe = Path(tmp) / 'test'
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: production BOM limits (6/11), SL isolation, absent region, AUX3 pending upload; picking declaration intact. No live GPU/network test.')
