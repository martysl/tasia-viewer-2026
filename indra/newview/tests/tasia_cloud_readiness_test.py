#!/usr/bin/env python3
"""Execute real self cloud checks with synthetic state; no viewer/network."""
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = (root / 'llvoavatarself.cpp').read_text()
start = source.index('bool LLVOAvatarSelf::getHasMissingParts() const')
end = source.index('\n}', start) + 2
function = source[start:end]
fixture = r'''
#include <cassert>
#include <sstream>
#include <string>
#include <vector>
using U32=unsigned; using S32=int; using F32=float;
using ETextureIndex=int; using EBakedTextureIndex=int;
enum {BAKED_LOWER=2, BAKED_UPPER=1, BAKED_SKIRT=4,
      TEX_HAIR=11, TEX_LOWER_BAKED=2, TEX_UPPER_BAKED=1, IMG_INVISIBLE=7};
namespace LLWearableType { enum EType {WT_SHAPE, WT_HAIR, WT_EYES, WT_SKIN, WT_SKIRT}; }
struct { int counts[5]={1,1,1,1,0}; int getWearableCount(int i) const {return counts[i];} } gAgentWearables;
struct LLTimer { float getElapsedTimeF32() const {return 31.f;} void reset() {} };
std::ostringstream messages;
#define LL_WARNS(category) messages
#define LL_INFOS() messages
#define LL_DEBUGS() messages
#define LL_ENDL '\n'
struct LLViewerTexture {
 bool ready=true; int id=1;
 bool hasGLTexture() const {return ready;}
 int getID() const {return id;}
};
struct LLVOAvatarSelf {
 struct BakedTextureData {int mTextureIndex;};
 std::vector<BakedTextureData> mBakedTextureDatas;
 LLViewerTexture images[12];
 bool defined[12], available[12];
 bool legacy=true, mPreviousFullyLoaded=false;
 LLVOAvatarSelf() {
  for(int i=0;i<12;++i) {defined[i]=true;available[i]=true; if(i<11)mBakedTextureDatas.push_back({i});}
 }
 bool usesLegacyBakes() const {return legacy;}
 bool isTextureDefined(int i,int) const {return defined[i];}
 bool isUsingLocalAppearance() const {return false;}
 bool hasPendingBakedUploads() const {return false;}
 const LLViewerTexture* getImage(int i,int) const {return &images[i];}
 int getLayerSet(int i) const {return i;}
 bool isLocalTextureDataAvailable(int i) const {return available[i];}
 bool isWearingWearableType(int i) const {return gAgentWearables.counts[i]!=0;}
 bool getHasMissingParts() const;
};
'''
main = r'''
int main() {
 LLVOAvatarSelf avatar;
 avatar.legacy=false;
 assert(!avatar.getHasMissingParts());
 assert(messages.str().find("Self readiness;")==std::string::npos);
 avatar.legacy=true;
 avatar.images[6].ready=false; avatar.images[6].id=IMG_INVISIBLE;
 assert(avatar.getHasMissingParts());
 assert(messages.str().find("slot=6; te=6; defined=1; image_present=1; gpu_ready=0; invisible=1")!=std::string::npos);
 avatar.images[6].ready=true;
 assert(!avatar.getHasMissingParts());
 gAgentWearables.counts[0]=0; assert(avatar.getHasMissingParts());
 gAgentWearables.counts[0]=1;
 avatar.defined[TEX_HAIR]=false; assert(avatar.getHasMissingParts());
 avatar.defined[TEX_HAIR]=true;
 avatar.defined[TEX_LOWER_BAKED]=false; avatar.available[BAKED_LOWER]=false;
 assert(avatar.getHasMissingParts());
 avatar.available[BAKED_LOWER]=true;
 assert(!avatar.getHasMissingParts());
 for(int i=0;i<20;++i) avatar.getHasMissingParts();
 auto text=messages.str(); std::size_t n=0, pos=0;
 while((pos=text.find("Self readiness;",pos))!=std::string::npos){++n;++pos;}
 assert(n==12);
 assert(text.find("sample=13;")==std::string::npos);
}
'''
with tempfile.TemporaryDirectory(prefix='tasia-cloud-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(fixture + function + main)
    exe = Path(tmp) / 'test'
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: real cloud checks preserve return values, report synthetic slot/GPU/invisible state, skip SL diagnostics and cap samples at 12; no runtime diagnosis claimed.')
