#!/usr/bin/env python3
"""Exercise the actual status-bar layout in local versus parent coordinates."""
from pathlib import Path
import unittest
from tasia_portable_branding_test import method
from tasia_sound_globe_test import compile_run

NV = Path(__file__).resolve().parents[1]


class StatusBarLayout(unittest.TestCase):
    def test_production_layout_fits_and_is_idempotent_after_resize(self):
        fixture = r'''
#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>
using S32=int;
template<class T>T llmin(T a,T b){return std::min(a,b);}
template<class T>T llmax(T a,T b){return std::max(a,b);}
struct LLRect { int mLeft=0,mTop=19,mRight=0,mBottom=0;
 int getWidth()const{return mRight-mLeft;} int getHeight()const{return mTop-mBottom;} };
struct LLView { using child_list_t=std::vector<LLView*>; LLRect rect;bool visible=true;bool right=true;
 LLRect getRect()const{return rect;}bool getVisible()const{return visible;}
 bool followsRight()const{return right;}
 void setRect(LLRect value){rect=value;}
 void translate(int x,int y){rect.mLeft+=x;rect.mRight+=x;rect.mTop+=y;rect.mBottom+=y;} };
struct LLStatusBar:LLView { child_list_t children;
  const child_list_t* getChildList(){return &children;}void setRect(LLRect value){int delta=value.getWidth()-rect.getWidth();rect=value;
   for(auto* child:children)if(child&&child->followsRight())child->translate(delta,0);}
  void reshapeToFitContent(); };
'''
        fixture += method((NV/'llstatusbar.cpp').read_text(), 'void LLStatusBar::reshapeToFitContent(')
        fixture += r'''
void check(LLStatusBar& bar){for(auto* child:bar.children)if(child&&child->visible){
 assert(child->rect.mLeft>=0);assert(child->rect.mRight<=bar.rect.getWidth());
 assert(bar.rect.mLeft+child->rect.mLeft>=0);assert(bar.rect.mLeft+child->rect.mRight<=bar.rect.mRight);
}}
int main(){
 LLStatusBar bar;bar.rect={645,19,1645,0};
 LLView balance,time,quic,volume,search;
 balance.rect={580,19,765,0};time.rect={765,19,850,0};quic.rect={854,19,872,0};
 volume.rect={1010,19,1026,0};search.rect={448,19,568,0};
 bar.children={&balance,&time,&quic,&volume,&search};
 bar.reshapeToFitContent();check(bar);
 assert(bar.rect.mRight==1645);assert(bar.rect.getWidth()<650);
 auto oldBar=bar.rect;auto oldVolume=volume.rect;
 for(int i=0;i<20;i++){bar.reshapeToFitContent();}
 check(bar);
 assert(bar.rect.mLeft==oldBar.mLeft&&volume.rect.mRight==oldVolume.mRight);
 bar.rect.mLeft-=400;bar.rect.mRight-=400;bar.reshapeToFitContent();check(bar);
 search.visible=false;bar.reshapeToFitContent();check(bar);
 search.visible=true;bar.reshapeToFitContent();check(bar);
 balance.rect.mLeft-=100;bar.reshapeToFitContent();check(bar);
}
'''
        compile_run(fixture)


if __name__ == '__main__':
    unittest.main(verbosity=2)
