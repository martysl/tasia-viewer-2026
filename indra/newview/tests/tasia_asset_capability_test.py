#!/usr/bin/env python3
"""Asset URL and mesh-capability regression tests; no real-session claims."""
from pathlib import Path
import unittest
from tasia_portable_branding_test import method
from tasia_sound_globe_test import compile_run

NV = Path(__file__).resolve().parents[1]


class AssetCapabilities(unittest.TestCase):
    def test_exact_opensim_paths_queries_and_sl_compatibility(self):
        compile_run(r'''
#include <cassert>
#include "lltasia_asset_caps.h"
int main(){using namespace TasiaAssetCaps;
 const std::string cap="https://assets.example:2005/opaque-cap";
 assert(assetURL(cap,"bodypart","id",true)==cap+"?bodypart_id=id");
 assert(assetURL(cap+"/","mesh","id",true)==cap+"/?mesh_id=id");
 assert(assetURL(cap+"?token=abc","mesh","id",true)==cap+"?token=abc&mesh_id=id");
 assert(assetURL(cap+"?","mesh","id",true)==cap+"?mesh_id=id");
 assert(assetURL(cap,"mesh","id",false)==cap+"/?mesh_id=id");
 assert(assetURL("","mesh","id",true).empty());
 assert(mesh("viewer","v2","v1",true)=="viewer");
 assert(mesh("","v2","v1",true)=="v2");
 assert(mesh("","","v1",true)=="v1");
 assert(mesh("","v2","v1",false).empty());
}
''')

    def test_actual_mesh_url_updates_without_using_region_name(self):
        source = (NV/'llmeshrepository.cpp').read_text()
        fixture = r'''
#include <cassert>
#include <sstream>
#include "lltasia_asset_caps.h"
struct LLUUID{std::string asString()const{return "mesh-id";}friend std::ostream& operator<<(std::ostream& s,const LLUUID&){return s<<"id";}};
struct Region{int getRegionID(){return 1;}} region;
struct Agent{bool present=true;Region* getRegion(){return present?&region:nullptr;}}gAgent;
struct LLMutexLock{explicit LLMutexLock(void*){}};
std::ostringstream logs;
#define LL_WARNS_ONCE(...) logs
#define LL_DEBUGS_ONCE(...) logs
#define LL_ENDL "\n"
struct LLMeshRepoThread{void* mMutex=nullptr;std::string mGetMeshCapability;bool mMeshCapabilityIsOpenSim=false;
 void setGetMeshCap(const std::string&,bool);void constructUrl(LLUUID,std::string*);};
'''
        fixture += method(source, 'void LLMeshRepoThread::setGetMeshCap(')
        fixture += method(source, 'void LLMeshRepoThread::constructUrl(')
        fixture += r'''
int main(){LLMeshRepoThread thread;std::string out;
 thread.setGetMeshCap("https://a.test/cap-old",true);thread.constructUrl({},&out);
 assert(out=="https://a.test/cap-old?mesh_id=mesh-id");
 thread.setGetMeshCap("https://a.test/cap-renewed",true);thread.constructUrl({},&out);
 assert(out=="https://a.test/cap-renewed?mesh_id=mesh-id");
 thread.setGetMeshCap("https://sl.test/cap",false);thread.constructUrl({},&out);
 assert(out=="https://sl.test/cap/?mesh_id=mesh-id");
 thread.setGetMeshCap("",true);thread.constructUrl({},&out);assert(out.empty());
 gAgent.present=false;thread.setGetMeshCap("old",true);thread.constructUrl({},&out);assert(out.empty());
}
'''
        compile_run(fixture)
        self.assertFalse('static std::string region_name' in source,
                         'Mesh capability is incorrectly keyed by a region display name')
        region = (NV/'llviewerregion.cpp').read_text()
        self.assertIn('capabilityNames.append("GetMesh2")', region)
        self.assertIn('capabilityNames.append("GetMesh")', region)

    def test_baked_texture_fetch_uses_grid_compatible_capability_url(self):
        source = (NV/'lltexturefetch.cpp').read_text()
        self.assertIn('#include "lltasia_asset_caps.h"', source)
        self.assertIn('TasiaAssetCaps::assetURL(http_url, "texture", mID.asString(),', source)
        self.assertIn('LLGridManager::getInstance()->isInOpenSim()', source)
        self.assertNotIn('setUrl(http_url + "/?texture_id="', source)

        compile_run(r'''
#include <cassert>
#include "lltasia_asset_caps.h"
int main(){using TasiaAssetCaps::assetURL;
 const std::string cap="https://assets.example:2005/opaque-cap";
 assert(assetURL(cap,"texture","baked-id",true)==cap+"?texture_id=baked-id");
 assert(assetURL(cap,"texture","baked-id",false)==cap+"/?texture_id=baked-id");
}
''')


if __name__ == '__main__':
    unittest.main(verbosity=2)
