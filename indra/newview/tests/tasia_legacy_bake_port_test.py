#!/usr/bin/env python3
"""Focused foundation checks; NOT proof of a complete local-bake port.

Compile the actual LLVisualParam class and its modified methods with minimal
dependency fixtures, then exercise flag forwarding. No network or GPU is used.
Counting reference comments/identifiers cannot establish functional parity.
"""

import re
import subprocess
import tempfile
from pathlib import Path

INDRA = Path(__file__).resolve().parents[2]


def read(rel):
    return (INDRA / rel).read_text()


def executable(src):
    return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"', '', src, flags=re.S)


def function(src, signature):
    start = src.index(signature)
    # Ignore commented-out old signatures.
    while src[src.rfind('\n', 0, start) + 1:start].lstrip().startswith('//'):
        start = src.index(signature, start + len(signature))
    opening = src.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (src[end] == '{') - (src[end] == '}')
        end += 1
    return src[start:end]


# Catch the missing braces which the old occurrence-count test passed.
for rel in (
    'llcharacter/llcharacter.h', 'llcharacter/llvisualparam.h',
    'llappearance/lltexlayerparams.h', 'llappearance/lldriverparam.cpp',
):
    depth = 0
    for char in executable(read(rel)):
        depth += (char == '{') - (char == '}')
        assert depth >= 0, f'Unbalanced closing brace: {rel}'
    assert depth == 0, f'Unclosed brace: {rel}'

# New overloads must be LIVE declarations, not comments as in the initial port.
params = executable(read('llappearance/lltexlayerparams.h'))
for name in ('setWeight', 'setAnimationTarget', 'animate'):
    assert len(re.findall(r'\b' + name + r'\(F32 \w+, bool upload_bake\) override;', params)) == 2, name
assert 'drivenp' not in executable(read('llappearance/lldriverparam.cpp'))
assert 'LLDXTexture' in read('llappearance/lltexlayerparams.h')

header = read('llcharacter/llvisualparam.h')
header = re.sub(r'^#include[^\n]*', '', header, flags=re.M)
source = read('llcharacter/llvisualparam.cpp')
methods = '\n'.join(function(source, signature) for signature in (
    'void LLVisualParam::setWeight(F32 weight, bool upload_bake)',
    'void LLVisualParam::setAnimationTarget(F32 target_value, bool upload_bake)',
    'void LLVisualParam::animate( F32 delta, bool upload_bake)',
    'void LLVisualParam::stopAnimating(bool upload_bake)',
))
fixture = r'''
#include <cassert>
#include <string>
#include <ostream>
#include <functional>
#include <algorithm>
using S32 = int;
using F32 = float;
#define LL_ALIGN_PREFIX(n)
#define LL_ALIGN_POSTFIX(n)
#define llassert assert
template<class T> T llclamp(T v, T low, T high) { return std::clamp(v, low, high); }
'''
support = r'''
LLVisualParam::LLVisualParam()
 : mCurWeight(0), mLastWeight(0), mNext(nullptr), mTargetWeight(0),
   mIsAnimating(false), mIsDummy(false), mID(0), mInfo(nullptr),
   mParamLocation(LOC_UNKNOWN) {}
LLVisualParam::~LLVisualParam() = default;
bool LLVisualParam::linkDrivenParams(visual_param_mapper, bool) { return true; }
void LLVisualParam::resetDrivenParams() {}
LLVisualParamInfo::LLVisualParamInfo()
 : mID(0), mGroup(VISUAL_PARAM_GROUP_TWEAKABLE), mMinWeight(0),
   mMaxWeight(1), mDefaultWeight(0), mSex(SEX_BOTH) {}
bool LLVisualParamInfo::parseXml(LLXmlTreeNode*) { return false; }
void LLVisualParamInfo::toStream(std::ostream&) {}
struct Probe : LLVisualParam {
    LLVisualParamInfo info;
    int calls = 0;
    bool flag = false;
    Probe() { mInfo = &info; }
    void apply(ESex) override {}
    S32 getWearableType() const override { return 0; }
    void setWeight(F32 weight, bool upload = false) override {
        ++calls; flag = upload; LLVisualParam::setWeight(weight, upload);
    }
    void setAnimationTarget(F32 target, bool upload = false) override {
        flag = upload; LLVisualParam::setAnimationTarget(target, upload);
    }
    void next(Probe* p) { mNext = p; }
    void detached() { mNext = nullptr; }
};
int main() {
    Probe first, second;
    first.next(&second);
    LLVisualParam* base = &first;
    base->setWeight(0.25f, true);
    assert(first.flag && second.flag && second.getWeight() == 0.25f);
    base->setWeight(0.5f); // Existing one-argument callers remain supported.
    assert(!first.flag && !second.flag);
    base->setAnimationTarget(0.75f, true);
    assert(first.flag && second.flag);
    base->animate(0.5f, false);
    assert(!first.flag);
    base->stopAnimating(true);
    assert(first.flag && !first.isAnimating());
    assert(first.getWeight() == 0.75f);
    first.detached();
}
'''
with tempfile.TemporaryDirectory(prefix='bake-foundation-', dir='/tmp/opencode') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    binary = Path(tmp) / 'test'
    cpp.write_text(fixture + header + methods + support)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

print('PASS: actual LLVisualParam header/methods compile; flag forwarding and legacy '
      'call compatibility execute correctly; braces and live override declarations checked. '
      'Full viewer/local bake remains unverified.')
