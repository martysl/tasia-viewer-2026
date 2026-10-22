#!/usr/bin/env python3
"""Production sound-stop path and 3D globe geometry, with no live GUI claims."""
from pathlib import Path
import subprocess
import tempfile
import unittest
import re
import xml.etree.ElementTree as ET
from tasia_portable_branding_test import method

ROOT = Path(__file__).resolve().parents[3]
NV = ROOT/'indra/newview'


def compile_run(source):
    with tempfile.TemporaryDirectory(dir='/tmp/opencode',prefix='sound-globe-') as tmp:
        cpp, exe = Path(tmp)/'test.cpp', Path(tmp)/'test'
        cpp.write_text(source)
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',
                        '-fsanitize=address,undefined','-I'+str(NV),str(cpp),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)


class SoundAndGlobe(unittest.TestCase):
    def test_actual_commit_callback_signatures(self):
        statements = []
        for filename, names in [('llfloatergridmanager.cpp',
                                 ['discover_grid','save_metadata','update_catalog'])]:
            source = (NV/filename).read_text()
            for name in names:
                pattern = (r'getChild<LL(?:Button|UICtrl)>\("'+name+r'"\)->set(?:Commit|Clicked)Callback\('
                           r'(\[[^\]]*\]\([^)]*\)|&[A-Za-z_:]+\([^)]*\))')
                found = re.search(pattern, source)
                self.assertIsNotNone(found, f'{filename}: {name}')
                signature = found.group(1)
                if signature.startswith('&') or 'boost::bind' in signature:
                    continue
                statements.append('std::function<void(LLUICtrl*,const LLSD&)> cb'+str(len(statements))+' = '+signature+' {};')
        self.assertTrue(statements, 'Expected at least one lambda callback to verify')
        compile_run('#include <functional>\nstruct LLUICtrl{};struct LLSD{};\n'
                    'struct Fixture{void test(){'+''.join(statements)+'}};\nint main(){Fixture f;f.test();}')

    def test_globe_projection_clipping_and_zoom(self):
        compile_run(r'''
#include "lltasia_globe_math.h"
#include <cassert>
#include <climits>
int main(){
 using namespace TasiaGlobe;
 for(int x=-180;x<=180;++x)for(int y=-90;y<=90;y+=3){
  auto p=project(x*pi/180,y*pi/180,.8,-.4);
  assert(std::abs(p.x*p.x+p.y*p.y+p.z*p.z-1)<1e-12);
 }
 assert(project(0,0,0,0).z>.99);assert(project(pi,0,0,0).z<-.99);
 auto a=project(.4,.7,0,0),b=project(.4,.7,2*pi,0);
 assert(std::abs(a.x-b.x)<1e-12 && std::abs(a.z-b.z)<1e-12);
 assert(zoom(1,INT_MAX)>=.65 && zoom(1,INT_MIN)<=1.8);
 assert(atlas(5,5,5,pi)==0);assert(atlas(-1,0,10,pi)==-pi);assert(atlas(11,0,10,pi)==pi);
 Vertex front{{0,1,1},0,0},back{{1,0,-1},1,1},back2{{-1,0,-1},0,1};
 auto poly=frontTriangle(front,back,back2);assert(poly.count==3);
 for(int i=0;i<poly.count;++i)assert(poly.points[i].p.z>=0 && std::isfinite(poly.points[i].u));
 assert(frontTriangle(back,back2,back).count==0);
 assert(frontTriangle(front,front,back).count==4);
}
''')

    def test_actual_audio_start_stop_blocks_delayed_restart(self):
        source = (NV/'llvieweraudio.cpp').read_text()
        fixture = r'''
#include <cassert>
#include <string>
#include <sstream>
using S32=int;
std::ostringstream logs;
#define LL_DEBUGS(...) logs
#define LL_WARNS(...) logs
#define LL_ENDL "\n"
constexpr int FMODEX_STREAM_BUFFER_SIZE=1,FMODEX_DECODE_BUFFER_SIZE=1;
struct LLStringUtil{static const std::string null;};const std::string LLStringUtil::null;
struct LLStreamingAudioInterface{bool supportsAdjustableBufferSizes(){return false;}void setBufferSizes(int,int){}};
struct Engine{int starts=0,stops=0;std::string uri;std::string getInternetStreamURL(){return uri;}
 LLStreamingAudioInterface* getStreamingAudioImpl(){return nullptr;}
 void startInternetStream(const std::string& value){uri=value;if(!value.empty())++starts;}
 void stopInternetStream(){++stops;uri.clear();}
}engine;Engine* gAudiop=&engine;
struct LLViewerAudio{
 enum Fade{FADE_IDLE,FADE_IN,FADE_OUT};Fade mFadeState=FADE_IDLE;
 bool mParcelAudioStoppedByUser=false,mExternalPlayerActive=false,mDone=false;
 std::string mNextStreamURI;
 void startFading(){}void registerIdleListener(){}
 void startInternetStreamWithAutoFade(const std::string&);
 void stopInternetStreamWithAutoFade();
};
'''
        fixture += method(source,'void LLViewerAudio::startInternetStreamWithAutoFade(')
        fixture += method(source,'void LLViewerAudio::stopInternetStreamWithAutoFade(')
        fixture += r'''
int main(){
 LLViewerAudio audio;audio.startInternetStreamWithAutoFade("stream-a");assert(engine.starts==1);
 audio.mParcelAudioStoppedByUser=true;audio.stopInternetStreamWithAutoFade();
 assert(engine.uri.empty() && audio.mNextStreamURI.empty());
 audio.startInternetStreamWithAutoFade("late-ask-reply");
 audio.startInternetStreamWithAutoFade("new-parcel-stream");assert(engine.starts==1);
 audio.startInternetStreamWithAutoFade("");assert(audio.mNextStreamURI.empty());
 audio.mParcelAudioStoppedByUser=false;audio.startInternetStreamWithAutoFade("stream-b");assert(engine.starts==2);
 audio.mExternalPlayerActive=true;audio.startInternetStreamWithAutoFade("parcel-c");assert(engine.starts==2);
 gAudiop=nullptr;audio.stopInternetStreamWithAutoFade();
}
'''
        compile_run(fixture)

    def test_native_ui_and_teleport_wiring(self):
        source = (NV/'llfloaterworldmap.cpp').read_text()
        self.assertIn('trackLocation(destination)', source)
        self.assertIn('sendMapBlockRequest(', source)
        globe = (NV/'lltasia_globe.cpp').read_text()
        self.assertIn('LLRender::TRIANGLES', globe)
        self.assertIn('frontTriangle(a,b,c)', globe)
        self.assertIn('if (p.z <= .02', globe)
        self.assertNotIn('teleportVia', globe) # selection must never itself teleport
        self.assertNotIn('http://', globe)
        self.assertNotIn('https://', globe)
        status = (NV/'llstatusbar.cpp').read_text()
        self.assertIn('reshapeToFitContent', status)
        media = (NV/'llviewermedia.cpp').read_text()
        self.assertIn('setParcelAudioStoppedByUser(val)',method(media,'void LLViewerMedia::setAllMediaPaused('))
        # The crowded bar must not grow a new button; the existing media toggle
        # and Nearby Media panel provide the stop action instead.
        for language in ('en','pl'):
            panel = (NV/f'skins/default/xui/{language}/panel_status_bar.xml').read_text()
            self.assertNotIn('stop_stream_btn', panel)
        for language in ('en','pl'):
            ET.parse(NV/f'skins/default/xui/{language}/floater_world_map.xml')
            ET.parse(NV/f'skins/default/xui/{language}/panel_status_bar.xml')


if __name__=='__main__':
    unittest.main(verbosity=2)
