// Win32 API stand-ins for the actual capture helper. Never reads the real clipboard.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using U8=unsigned char;using U32=std::uint32_t;using U64=std::uint64_t;
using SIZE_T=size_t;using UINT=unsigned;using HANDLE=void*;using HBITMAP=void*;using HDC=void*;
using LLFILE=FILE;
constexpr int CF_BITMAP=2,BI_RGB=0,DIB_RGB_COLORS=0;
struct BITMAP{int bmWidth=2,bmHeight=1;};
struct BITMAPINFOHEADER{unsigned biSize=0;int biWidth=0,biHeight=0;unsigned short biPlanes=0,biBitCount=0;unsigned biCompression=0;};
struct BITMAPINFO{BITMAPINFOHEADER bmiHeader;};
bool open_ok=true,png_present=true,bitmap_present=false,dc_ok=true,rows_ok=true;
int opened=0,closed=0,locks=0,unlocks=0;
std::vector<U8> png_data,encoded_pixels;
BITMAP bitmap_info;
bool OpenClipboard(void*){if(!open_ok)return false;++opened;return true;}
void CloseClipboard(){++closed;}
UINT RegisterClipboardFormatW(const wchar_t*){return 9;}
HANDLE GetClipboardData(UINT format){
 if(format==9)return png_present?&png_data:nullptr;
 return bitmap_present?&bitmap_info:nullptr;
}
SIZE_T GlobalSize(HANDLE){return png_data.size();}
void* GlobalLock(HANDLE){++locks;return png_data.data();}
void GlobalUnlock(HANDLE){++unlocks;}
int GetObject(HBITMAP,int,BITMAP* b){*b=bitmap_info;return sizeof(*b);}
HDC GetDC(void*){return dc_ok?&bitmap_info:nullptr;}
void ReleaseDC(void*,HDC){}
int GetDIBits(HDC,HBITMAP,unsigned,unsigned rows,void* p,BITMAPINFO*,unsigned){
 if(!rows_ok)return 0;
 const U8 pixels[]={3,2,1,0,6,5,4,0};std::memcpy(p,pixels,sizeof(pixels));return rows;
}
struct LLFile{
 static LLFILE* fopen(const std::string& p,const char* mode){return std::fopen(p.c_str(),mode);}
 static void remove(const std::string& p){std::remove(p.c_str());}
};
template<class T>struct LLPointer:std::shared_ptr<T>{
 LLPointer(T* p):std::shared_ptr<T>(p){}operator T*()const{return this->get();}
};
struct LLImageRaw{
 std::vector<U8> pixels;
 LLImageRaw(int w,int h,int c):pixels(w*h*c){}
 U8* getData(){return pixels.data();}
};
struct LLImagePNG{
 bool encode(const LLImageRaw* p,float){encoded_pixels=p->pixels;return true;}
 bool save(const std::string& p){auto f=LLFile::fopen(p,"wb");if(!f)return false;fputs("synthetic",f);fclose(f);return true;}
};
// Production capture_clipboard_png() is inserted by the Python runner.
