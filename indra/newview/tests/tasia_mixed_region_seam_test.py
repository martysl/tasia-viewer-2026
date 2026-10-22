#!/usr/bin/env python3
"""Compile the production partial terrain-neighbor methods against patch stubs."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
view = root / "indra/newview"
source_text = (view / "llsurface.cpp").read_text()


def function(signature):
    start = source_text.index(signature)
    opening = source_text.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source_text[end] == "{") - (source_text[end] == "}")
        end += 1
    return source_text[start:end]


methods = "\n".join(function(signature) for signature in (
    "void LLSurface::trackNeighbor(",
    "void LLSurface::connectNeighborEdge(",
    "void LLSurface::connectNeighborCorner(",
    "void LLSurface::disconnectNeighbor(",
    "void LLSurface::disconnectAllNeighbors("))

preamble = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
using U32 = std::uint32_t;
using S32 = std::int32_t;
constexpr U32 EAST=0, NORTH=1, WEST=2, SOUTH=3;
constexpr U32 NORTHEAST=4, NORTHWEST=5, SOUTHWEST=6, SOUTHEAST=7;
constexpr U32 gDirOpposite[8]={2,3,0,1,6,7,4,5};
#define LL_WARNS(...) std::cerr
#define LL_ENDL '\n'
class LLSurface;
class LLSurfacePatch {
public:
    LLSurface* mSurface=nullptr;
    std::array<LLSurfacePatch*,8> mNeighbor{};
    int mEastUpdates=0, mNorthUpdates=0, mDirty=0;
    LLSurfacePatch* getNeighborPatch(U32 d) const { return mNeighbor[d]; }
    void connectNeighbor(LLSurfacePatch* p, U32 d) { mNeighbor[d]=p; }
    void updateEastEdge() { ++mEastUpdates; }
    void updateNorthEdge() { ++mNorthUpdates; }
    void dirtyZ() { ++mDirty; }
    void disconnectNeighbor(LLSurface* surface);
};
class LLSurface {
public:
    struct NeighborConnection { LLSurface* mSurface; U32 mDirection; };
    S32 mPatchesPerEdge, mNumberOfPatches;
    LLSurfacePatch* mPatchList;
    LLSurface* mNeighbors[8]{};
    std::vector<NeighborConnection> mNeighborConnections;
    std::vector<LLSurfacePatch> mPatches;
    explicit LLSurface(S32 edge) : mPatchesPerEdge(edge), mNumberOfPatches(edge*edge),
        mPatchList(nullptr), mPatches(size_t(edge)*edge) {
        mPatchList=mPatches.data();
        for (auto& patch : mPatches) patch.mSurface=this;
    }
    LLSurfacePatch* getPatch(S32 x,S32 y) { assert(x>=0 && x<mPatchesPerEdge && y>=0 && y<mPatchesPerEdge); return &mPatches[size_t(x)+size_t(y)*mPatchesPerEdge]; }
    void trackNeighbor(LLSurface*,U32);
    void connectNeighborEdge(LLSurface*,U32,U32,U32,U32);
    void connectNeighborCorner(LLSurface*,U32);
    void disconnectNeighbor(LLSurface*);
    void disconnectAllNeighbors();
};
void LLSurfacePatch::disconnectNeighbor(LLSurface* surface) {
    for (auto& neighbor : mNeighbor) if (neighbor && neighbor->mSurface==surface) neighbor=nullptr;
}
'''

tests = r'''
int main() {
    LLSurface large(32), small_south(16), small_north(16);
    large.connectNeighborEdge(&small_south,EAST,0,0,16);
    large.connectNeighborEdge(&small_north,EAST,16,0,16);
    large.connectNeighborEdge(&small_south,EAST,0,0,16);
    assert(large.getPatch(31,0)->getNeighborPatch(EAST)==small_south.getPatch(0,0));
    assert(large.getPatch(31,16)->getNeighborPatch(EAST)==small_north.getPatch(0,0));
    assert(large.getPatch(31,15)->getNeighborPatch(NORTHEAST)==small_north.getPatch(0,0));
    assert(large.getPatch(31,16)->getNeighborPatch(SOUTHEAST)==small_south.getPatch(0,15));
    assert(small_north.getPatch(0,0)->getNeighborPatch(SOUTHWEST)==large.getPatch(31,15));
    assert(small_south.getPatch(0,15)->getNeighborPatch(NORTHWEST)==large.getPatch(31,16));
    assert(large.mNeighbors[EAST]==&small_south);
    assert(large.mNeighborConnections.size()==2);
    assert(large.getPatch(31,0)->mEastUpdates==2 && large.getPatch(31,0)->mDirty==2);

    // The inverse insertion order must produce the same junction diagonals.
    LLSurface large_loaded_last(32), lower_loaded_first(16), upper_loaded_first(16);
    lower_loaded_first.connectNeighborEdge(&large_loaded_last,WEST,0,0,16);
    upper_loaded_first.connectNeighborEdge(&large_loaded_last,WEST,0,16,16);
    assert(large_loaded_last.getPatch(31,16)->getNeighborPatch(SOUTHEAST)==lower_loaded_first.getPatch(0,15));
    assert(large_loaded_last.getPatch(31,15)->getNeighborPatch(NORTHEAST)==upper_loaded_first.getPatch(0,0));

    // Four 256m neighbors tile one 1024m side with consistent junction links.
    LLSurface very_large(64), tile0(16), tile1(16), tile2(16), tile3(16);
    LLSurface* tiles[4]={&tile0,&tile1,&tile2,&tile3};
    for (U32 i=0;i<4;++i)
        very_large.connectNeighborEdge(tiles[i],EAST,i*16,0,16);
    assert(very_large.mNeighborConnections.size()==4);
    assert(very_large.mNeighbors[EAST]==&tile0);
    for (U32 i=0;i<4;++i) {
        assert(very_large.getPatch(63,i*16)->getNeighborPatch(EAST)==tiles[i]->getPatch(0,0));
        if (i>0)
            assert(very_large.getPatch(63,i*16-1)->getNeighborPatch(NORTHEAST)==tiles[i]->getPatch(0,0));
        if (i<3)
            assert(very_large.getPatch(63,(i+1)*16)->getNeighborPatch(SOUTHEAST)==tiles[i]->getPatch(0,15));
    }

    // Removing the first smaller region must preserve the second relation and
    // clear both directions of every patch pointer to the removed surface.
    small_south.disconnectAllNeighbors();
    assert(large.mNeighbors[EAST]==&small_north);
    assert(large.getPatch(31,0)->getNeighborPatch(EAST)==nullptr);
    assert(large.getPatch(31,16)->getNeighborPatch(EAST)==small_north.getPatch(0,0));
    assert(small_south.mNeighborConnections.empty());

    large.disconnectAllNeighbors();
    assert(large.mNeighborConnections.empty() && large.mNeighbors[EAST]==nullptr);
    assert(small_north.mNeighborConnections.empty() && small_north.mNeighbors[WEST]==nullptr);
    assert(large.getPatch(31,16)->getNeighborPatch(EAST)==nullptr);

    LLSurface large_west(32), small_west(16);
    large_west.connectNeighborEdge(&small_west,WEST,4,4,8);
    assert(large_west.getPatch(0,4)->getNeighborPatch(WEST)==small_west.getPatch(15,4));
    assert(small_west.getPatch(15,4)->getNeighborPatch(EAST)==large_west.getPatch(0,4));
    assert(large_west.getPatch(0,4)->getNeighborPatch(NORTHWEST)==small_west.getPatch(15,5));
    assert(large_west.getPatch(0,3)->getNeighborPatch(NORTHWEST)==small_west.getPatch(15,4));
    assert(large_west.getPatch(0,12)->getNeighborPatch(SOUTHWEST)==small_west.getPatch(15,11));
    assert(small_west.getPatch(15,4)->getNeighborPatch(SOUTHEAST)==large_west.getPatch(0,3));
    assert(small_west.getPatch(15,11)->getNeighborPatch(NORTHEAST)==large_west.getPatch(0,12));

    LLSurface large_north(32), small_north_edge(16);
    large_north.connectNeighborEdge(&small_north_edge,NORTH,16,0,16);
    assert(large_north.getPatch(16,31)->getNeighborPatch(NORTH)==small_north_edge.getPatch(0,0));
    assert(large_north.getPatch(16,31)->getNeighborPatch(NORTHEAST)==small_north_edge.getPatch(1,0));
    LLSurface north_junction(32), small_north_junction(16);
    north_junction.connectNeighborEdge(&small_north_junction,NORTH,8,0,8);
    assert(north_junction.getPatch(7,31)->getNeighborPatch(NORTHEAST)==small_north_junction.getPatch(0,0));
    assert(north_junction.getPatch(16,31)->getNeighborPatch(NORTHWEST)==small_north_junction.getPatch(7,0));
    assert(small_north_junction.getPatch(0,0)->getNeighborPatch(SOUTHWEST)==north_junction.getPatch(7,31));
    assert(small_north_junction.getPatch(7,0)->getNeighborPatch(SOUTHEAST)==north_junction.getPatch(16,31));

    LLSurface large_south(32), small_south_edge(16);
    large_south.connectNeighborEdge(&small_south_edge,SOUTH,0,0,16);
    assert(large_south.getPatch(7,0)->getNeighborPatch(SOUTH)==small_south_edge.getPatch(7,15));
    assert(large_south.getPatch(7,0)->getNeighborPatch(SOUTHEAST)==small_south_edge.getPatch(8,15));
    LLSurface south_junction(32), small_south_junction(16);
    south_junction.connectNeighborEdge(&small_south_junction,SOUTH,8,0,8);
    assert(south_junction.getPatch(7,0)->getNeighborPatch(SOUTHEAST)==small_south_junction.getPatch(0,15));
    assert(south_junction.getPatch(16,0)->getNeighborPatch(SOUTHWEST)==small_south_junction.getPatch(7,15));
    assert(small_south_junction.getPatch(0,15)->getNeighborPatch(NORTHWEST)==south_junction.getPatch(7,0));
    assert(small_south_junction.getPatch(7,15)->getNeighborPatch(NORTHEAST)==south_junction.getPatch(16,0));

    LLSurface conflict_large(32), established(16), rejected(16);
    conflict_large.connectNeighborEdge(&established,EAST,0,0,16);
    conflict_large.connectNeighborEdge(&rejected,EAST,0,0,16);
    assert(conflict_large.mNeighbors[EAST]==&established);
    assert(conflict_large.mNeighborConnections.size()==1);
    assert(rejected.mNeighborConnections.empty());

    LLSurface invalid(32), invalid_neighbor(16);
    invalid.connectNeighborEdge(&invalid_neighbor,EAST,30,0,4);
    assert(invalid.mNeighborConnections.empty() && invalid_neighbor.mNeighborConnections.empty());

    const U32 directions[4]={NORTHEAST,NORTHWEST,SOUTHEAST,SOUTHWEST};
    const S32 ax[4]={31,0,31,0}, ay[4]={31,31,0,0};
    const S32 bx[4]={0,15,0,15}, by[4]={0,0,15,15};
    for (U32 i=0;i<4;++i) {
        LLSurface corner_a(32), corner_b(16), corner_conflict(16);
        corner_a.connectNeighborCorner(&corner_b,directions[i]);
        corner_a.connectNeighborCorner(&corner_b,directions[i]);
        assert(corner_a.mNeighborConnections.size()==1);
        assert(corner_b.mNeighborConnections.size()==1);
        assert(corner_a.mNeighbors[directions[i]]==&corner_b);
        assert(corner_a.getPatch(ax[i],ay[i])->getNeighborPatch(directions[i])==corner_b.getPatch(bx[i],by[i]));
        assert(corner_b.getPatch(bx[i],by[i])->getNeighborPatch(gDirOpposite[directions[i]])==corner_a.getPatch(ax[i],ay[i]));
        if (i==0) {
            corner_a.connectNeighborCorner(&corner_conflict,directions[i]);
            assert(corner_a.mNeighborConnections.size()==1);
            assert(corner_conflict.mNeighborConnections.empty());
        }
        corner_a.disconnectAllNeighbors();
        assert(corner_b.mNeighborConnections.empty());
        assert(corner_b.getPatch(bx[i],by[i])->getNeighborPatch(gDirOpposite[directions[i]])==nullptr);
    }
}
'''

with tempfile.TemporaryDirectory(prefix="tasia-mixed-seam-") as directory:
    cpp = Path(directory) / "seam.cpp"
    exe = Path(directory) / "seam"
    cpp.write_text(preamble + methods + tests)
    subprocess.run(shlex.split(os.environ.get("CXX", "c++")) + [
        "-std=c++17", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)
    ], check=True)
    result = subprocess.run([str(exe)], check=True, text=True, capture_output=True)
    warnings = result.stderr.strip().splitlines()
    assert warnings.count("Rejecting invalid partial terrain neighbor span") == 1
    assert warnings.count("Skipping overlapping terrain patch neighbor") == 17
    assert warnings.count("Skipping conflicting terrain corner neighbor") == 1
print("PASS: partial edge pairing; endpoint junction diagonals; multi-neighbor teardown; corner links")
