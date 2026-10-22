#!/usr/bin/env python3
"""Compile the production bake-pixel helper with deterministic D3D fixtures.

Exercises actual bounds/format/channel handling, NOT a real GPU or J2C codec.
"""
import subprocess
import tempfile
from pathlib import Path

indra = Path(__file__).resolve().parents[2]
stub = r'''
#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
enum DXGI_FORMAT { DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
 DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_BC7_UNORM };
struct D3D11_TEXTURE2D_DESC {
 unsigned Width=2, Height=2; DXGI_FORMAT Format=DXGI_FORMAT_R8G8B8A8_UNORM;
 struct { unsigned Count=1; } SampleDesc;
};
struct ID3D11Texture2D {
 D3D11_TEXTURE2D_DESC desc;
 std::vector<std::uint8_t> bytes = {1,2,3,4, 5,6,7,8, 99,99,99,99,
                                  9,10,11,12, 13,14,15,16, 99,99,99,99};
 bool fail=false; int calls=0;
 void GetDesc(D3D11_TEXTURE2D_DESC* out) { *out=desc; }
};
struct LLRenderTarget {
 ID3D11Texture2D* texture;
 ID3D11Texture2D* getDXColorTexture(unsigned) { return texture; }
};
struct DXReadback {
 static bool readPixels(ID3D11Texture2D* src, int x, int y, int w, int h, int bpp, void* dst) {
  ++src->calls;
  if (src->fail) return false;
  // Padded source pitch; the real DXReadback similarly removes RowPitch.
  for (int row=0; row<h; ++row)
   std::memcpy(static_cast<std::uint8_t*>(dst)+row*w*bpp,
               src->bytes.data()+(y+row)*12+x*bpp,w*bpp);
  return true;
 }
};
'''
test = r'''
#include "llbakereadback.h"
#include "lltasia_bake_upload.h"
#include <array>
#include <cassert>
#include <climits>
int main() {
 std::size_t count=100;
 assert(!LLBake::pixelCount(-1,2,count) && count==0);
 assert(!LLBake::pixelCount(INT_MAX,INT_MAX,count));
 assert(!LLBake::pixelCount(0,2,count));
 assert(LLBake::pixelCount(2048,2048,count) && count==4194304);
 ID3D11Texture2D texture;
 LLRenderTarget target{&texture};
 std::vector<std::uint8_t> rgba;
 assert(LLBake::readColor(&target,0,0,2,2,rgba));
  assert((rgba==std::vector<std::uint8_t>{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}));
  LLBake::PixelSummary summary;
   assert(LLBake::summarizeRGBA(rgba.data(),rgba.size(),2,2,summary));
    assert(summary.pixels==4 && summary.non_black==4 && summary.nontransparent_black==0 && summary.transparent==0);
    assert(!LLBake::isAllBlackNonTransparent(summary));
    TasiaBake::UploadPolicy visible_head_upload;
    visible_head_upload.request();
    const auto visible_ticket=visible_head_upload.begin();
    bool visible_head_assigned=false;
    if (!LLBake::isAllBlackNonTransparent(summary)) {
     visible_head_assigned=visible_head_upload.complete(visible_ticket,true)==
      TasiaBake::UploadPolicy::Result::Accepted;
    }
    assert(visible_head_assigned && !visible_head_upload.pending());
    const std::array<std::uint8_t,16> black_pixels{0,0,0,255, 0,0,0,0, 4,0,0,128, 0,0,0,255};
   assert(LLBake::summarizeRGBA(black_pixels.data(),black_pixels.size(),2,2,summary));
   assert(summary.pixels==4 && summary.non_black==1 && summary.nontransparent_black==2 && summary.transparent==1);
   assert(!LLBake::isAllBlackNonTransparent(summary));
   const std::array<std::uint8_t,16> opaque_black_pixels{0,0,0,255, 0,0,0,255, 0,0,0,255, 0,0,0,255};
   assert(LLBake::summarizeRGBA(opaque_black_pixels.data(),opaque_black_pixels.size(),2,2,summary));
   assert(LLBake::isAllBlackNonTransparent(summary));
   const std::array<std::uint8_t,16> partial_alpha_black_pixels{0,0,0,128, 0,0,0,128, 0,0,0,128, 0,0,0,128};
   assert(LLBake::summarizeRGBA(partial_alpha_black_pixels.data(),partial_alpha_black_pixels.size(),2,2,summary));
   assert(summary.nontransparent_black==4 && summary.transparent==0);
   assert(LLBake::isAllBlackNonTransparent(summary));
   TasiaBake::UploadPolicy head_upload;
   head_upload.request();
   const auto head_ticket=head_upload.begin();
   bool head_assigned=false;
   if (LLBake::isAllBlackNonTransparent(summary)) {
    assert(head_upload.complete(head_ticket,false)==TasiaBake::UploadPolicy::Result::Retry);
   } else {
    head_assigned=true;
   }
   assert(!head_assigned && head_upload.pending() && head_upload.failures()==1);
   assert(!LLBake::summarizeRGBA(black_pixels.data(),15,2,2,summary));
  std::array<std::uint8_t,6> alpha{90,0,0,0,0,91};
 assert(LLBake::copyAlpha(rgba.data(),rgba.size(),alpha.data()+1,4,2,2));
 assert((alpha==std::array<std::uint8_t,6>{90,4,8,12,16,91}));
 assert(!LLBake::copyAlpha(rgba.data(),15,alpha.data()+1,4,2,2));
 assert(!LLBake::copyAlpha(rgba.data(),16,alpha.data()+1,3,2,2));
 assert(!LLBake::copyAlpha(nullptr,16,alpha.data()+1,4,2,2));
 std::array<std::uint8_t,22> packed{}; packed.front()=81; packed.back()=82;
 assert(LLBake::packFiveChannels(rgba.data(),16,alpha.data()+1,4,packed.data()+1,20,2,2));
 assert((packed==std::array<std::uint8_t,22>{81,1,2,3,4,4,5,6,7,8,8,9,10,11,12,12,13,14,15,16,16,82}));
 auto before=packed;
 assert(!LLBake::packFiveChannels(rgba.data(),16,alpha.data()+1,3,packed.data()+1,20,2,2));
 assert(!LLBake::packFiveChannels(rgba.data(),16,alpha.data()+1,4,packed.data()+1,19,2,2));
 assert(packed==before);
 texture.desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
 assert(LLBake::readColor(&target,0,0,1,2,rgba));
 assert((rgba==std::vector<std::uint8_t>{3,2,1,4,11,10,9,12}));
 int calls=texture.calls;
 texture.desc.Format=DXGI_FORMAT_BC7_UNORM;
 assert(!LLBake::readColor(&target,0,0,2,2,rgba) && rgba.empty());
 assert(texture.calls==calls);
 texture.desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
 texture.desc.SampleDesc.Count=4;
 assert(!LLBake::readColor(&target,0,0,2,2,rgba));
 texture.desc.SampleDesc.Count=1;
 assert(!LLBake::readColor(&target,1,0,2,2,rgba));
 assert(!LLBake::readColor(&target,-1,0,1,1,rgba));
 assert(!LLBake::readColor(&target,INT_MAX,0,1,1,rgba));
 assert(!LLBake::readColor(nullptr,0,0,1,1,rgba));
 target.texture=nullptr;
 assert(!LLBake::readColor(&target,0,0,1,1,rgba));
 target.texture=&texture; texture.fail=true;
 assert(!LLBake::readColor(&target,0,0,2,2,rgba) && rgba.empty());
}
'''
with tempfile.TemporaryDirectory(prefix='bake-readback-', dir='/tmp/opencode') as tmp:
    tmp = Path(tmp)
    (tmp / 'llbakereadback.h').write_bytes((indra / 'llrender/llbakereadback.h').read_bytes())
    (tmp / 'lltasia_bake_upload.h').write_bytes((indra / 'newview/lltasia_bake_upload.h').read_bytes())
    (tmp / 'llrendertarget.h').write_text(stub)
    (tmp / 'DXReadback.h').write_text('#include "llrendertarget.h"\n')
    (tmp / 'test.cpp').write_text(test)
    subprocess.run(['g++', '-std=c++17', '-DDX_RENDER', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    str(tmp / 'test.cpp'), '-o', str(tmp / 'test')], check=True)
    subprocess.run([str(tmp / 'test')], check=True)

src = (indra / 'llappearance/lltexlayer.cpp').read_text()
assert 'LLBake::readColor(bound_target, x, y, width, height, rgba)' in src
assert 'mask not cached' in src
upload_src = (indra / 'newview/llviewertexlayer.cpp').read_text()
assert 'LLBake::summarizeRGBA(' in upload_src
assert 'LLBake::isAllBlackNonTransparent(summary)' in upload_src
assert 'bake_slot == LLAvatarAppearanceDefines::BAKED_HEAD' in upload_src
start = upload_src.index('void LLViewerTexLayerSetBuffer::doUpload()')
opening = upload_src.index('{', start)
end, depth = opening + 1, 1
while depth:
    depth += (upload_src[end] == '{') - (upload_src[end] == '}')
    end += 1
do_upload = upload_src[start:end]
black_guard = do_upload.index('if (head_bake && have_pixel_summary && LLBake::isAllBlackNonTransparent(summary))')
encode_buffer = do_upload.index('LLPointer<LLImageRaw> baked', black_guard)
assert 'completion->complete(false);' in do_upload[black_guard:encode_buffer]
assert 'return;' in do_upload[black_guard:encode_buffer]
start = upload_src.index('void LLViewerTexLayerSetBuffer::processUploadCompletion()')
opening = upload_src.index('{', start)
end, depth = opening + 1, 1
while depth:
    depth += (upload_src[end] == '{') - (upload_src[end] == '}')
    end += 1
completion = upload_src[start:end]
accepted = completion.index('if (result == TasiaBake::UploadPolicy::Result::Accepted)')
retry = completion.index('else if (result == TasiaBake::UploadPolicy::Result::Retry)')
assert 'setNewBakedTexture(' in completion[accepted:retry]
assert 'mUploadRetryTimer.start();' in completion[retry:]
assert '"; slot=" << bake_slot' in upload_src
assert '"; region=" << layer->getBodyRegionName()' in upload_src
assert '"; nontransparent_black=" << summary.nontransparent_black' in upload_src
assert 'LL_VLOGS(LLError::LEVEL_INFO, "LocalBake")' in upload_src
assert 'LL_DISABLE_INFOS' in (indra / 'llcommon/llerror.h').read_text()
assert 'std::array<U32, LLAvatarAppearanceDefines::BAKED_NUM_INDICES>' in upload_src
assert 'pixel_summary_samples[bake_slot] < 2' in upload_src
assert 'black_head_diagnostic_samples < 2' in upload_src
assert 'TEX_HEAD_BODYPAINT' in upload_src and 'TEX_HEAD_UNIVERSAL_TATTOO' in upload_src
assert 'getTEWearableType(texture_index)' in upload_src
assert 'gAgentWearables.getWearableCount(wearable_type)' in upload_src
assert 'llmin(wearable_count, MAX_DIAGNOSTIC_WEARABLES)' in upload_src
assert 'avatar->getLocalDiscardLevel(texture_index, wearable_index)' in upload_src
assert 'avatar->isTextureDefined(texture_index, wearable_index)' in upload_src
assert 'wearable_scan_capped=' in upload_src
assert 'getID()' not in upload_src[black_guard:encode_buffer]
print('PASS: production bake helper: RGBA/BGRA, padded pitch fixture, alpha/five-channel '
      'packing, aggregate pixel summaries, bounds/canaries/failures, ASan+UBSan. '
      'Black-head input diagnostics are bounded/source-checked. Real GPU/J2C untested.')
