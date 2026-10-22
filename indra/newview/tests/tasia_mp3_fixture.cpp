// Fake viewer APIs around the production MP3 translation unit; no network or audio upload.
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
using S32=int; using S64=long long; using F32=float;
std::string work;
bool gDisconnected=false;
struct LLApp{static bool isQuitting(){return false;}};
template<class... Args> std::string llformat(const char* f, Args... args) {
    char text[512]; std::snprintf(text,sizeof(text),f,args...); return text;
}
struct LLUUID {
    std::string value;
    bool notNull() const { return !value.empty(); }
    bool operator==(const LLUUID& b) const { return value==b.value; }
};
struct LLSD {
    std::map<std::string,LLSD> values;
    std::string text;
    int number=0;
    LLSD& operator[](const std::string& s) { return values[s]; }
    LLSD& operator=(int n) { number=n; return *this; }
    LLSD& operator=(const std::string& s) { text=s; return *this; }
    LLUUID asUUID() const { return {text}; }
};
struct LLFile {
    static int mkdir(const std::string& p) { return std::filesystem::create_directory(p) ? 0 : 1; }
    static void remove(const std::string& p) { std::filesystem::remove(p); }
    static void rmdir(const std::string& p) { std::filesystem::remove(p); }
};
struct Dir {
    int serial=0; bool collide=false; std::string last;
    std::string getTempFilename() { last=work+(collide?"/existing":"/batch-"+std::to_string(++serial)); return last; }
    std::string getExtension(const std::string& s) { return s.substr(s.find_last_of('.')+1); }
    std::string getBaseFileName(const std::string&,bool) { return "Synthetic"; }
    std::string getExecutableDir() { return work; }
    std::string add(const std::string& path, const std::string& name) const { return path + "/" + name; }
    void append(std::string& path, const std::string& name) const { path = add(path, name); }
    bool fileExists(const std::string& s) { return std::filesystem::exists(s); }
} directory;
Dir* gDirUtilp=&directory;
struct LLDirIterator {
    std::vector<std::string> entries; size_t i=0;
    LLDirIterator(const std::string& p,const std::string&) {
        if(std::filesystem::exists(p)) for(auto& f:std::filesystem::directory_iterator(p))
            if(f.path().extension()==".wav") entries.push_back(f.path().filename().string());
    }
    bool next(std::string& s) { if(i==entries.size()) return false; s=entries[i++]; return true; }
};
struct LLStringUtil {
    static void toLower(std::string& s) { for(char& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
    static void replaceNonstandardASCII(std::string&,char) {}
    static void replaceChar(std::string&,char,char) {}
    static void stripNonprintable(std::string&) {}
};
struct Agent {
    LLUUID id{"agent"},session{"session"}; std::string capability="synthetic-capability";
    LLUUID getID() const { return id; } LLUUID getSessionID() const { return session; }
    std::string getRegionCapability(const std::string&) const { return capability; }
} gAgent;
struct Camera { bool cameraMouselook(){return false;} void changeCameraToDefault(){} } gAgentCamera;
struct LLGridManager {
    std::string grid="synthetic-grid";
    static LLGridManager* getInstance(){static LLGridManager g;return &g;}
    std::string getGrid() const{return grid;}
};
namespace LLAssetType { enum EType{AT_SOUND}; }
namespace LLFolderType { enum EType{FT_SOUND}; }
namespace LLInventoryType { enum EType{IT_SOUND}; }
struct LLFloaterPerms {
    static int getNextOwnerPerms(const char*){return 0;}
    static int getGroupPerms(const char*){return 0;}
    static int getEveryonePerms(const char*){return 0;}
};
struct LLAgentBenefitsMgr {
    int cost=10; bool available=true;
    static LLAgentBenefitsMgr& current(){static LLAgentBenefitsMgr b;return b;}
    bool findUploadCost(LLAssetType::EType&,S32& out){out=cost;return available;}
};
struct Balance{int balance=100;int getBalance(){return balance;}} balance;
Balance* gStatusBar=&balance;
struct LLUICtrl { template<class T>void setValue(const T&){} };
struct LLProgressBar:LLUICtrl{};
struct LLFloater { template<class T>T* getChild(const char*){static T control;return &control;} };
struct LLFloaterReg {
    static LLFloater* showInstance(const char*){static LLFloater f;return &f;}
    static void hideInstance(const char*){}
};
struct LLTrans{static std::string getString(const std::string& s){return s;}};
struct LLNotificationsUtil {
    static std::string last; static LLSD args;
    static std::function<void(const LLSD&,const LLSD&)> confirm;
    static void add(const std::string& s){last=s;}
    static void add(const std::string& s,const LLSD& a){last=s;args=a;}
    template<class Callback>static void add(const std::string& s,const LLSD& a,const LLSD&,Callback cb){last=s;args=a;confirm=cb;}
    static int getSelectedOption(const LLSD&,const LLSD& response){return response.number;}
};
std::string LLNotificationsUtil::last;
LLSD LLNotificationsUtil::args;
std::function<void(const LLSD&,const LLSD&)> LLNotificationsUtil::confirm;
std::vector<std::function<bool()>> repeating;
std::vector<std::function<void()>> once;
template<class F>void doOnIdleRepeating(F f){repeating.emplace_back(f);}
template<class F>void doOnIdleOneTime(F f){once.emplace_back(f);}
struct LLTimer{static float elapsed;void reset(){elapsed=0;}float getElapsedTimeF32(){return elapsed;}};
float LLTimer::elapsed=0;
struct LLProcess {
    enum{UNSTARTED,RUNNING,EXITED,KILLED};
    struct Status{int mState=EXITED,mData=0;};
    struct Params {
        std::string executable,desc;
        struct Args{std::vector<std::string> values;void add(const std::string& s){values.push_back(s);}}args;
    };
    static Params last; static Status status; static bool running,killed;
    static std::shared_ptr<LLProcess> create(const Params& p){last=p;return std::make_shared<LLProcess>();}
    bool isRunning(){return running;} Status getStatus(){return status;}
    void kill(const char*){killed=true;running=false;}
};
LLProcess::Params LLProcess::last;
LLProcess::Status LLProcess::status;
bool LLProcess::running=false;bool LLProcess::killed=false;
using LLProcessPtr=std::shared_ptr<LLProcess>;
constexpr float LLVORBIS_CLIP_MAX_TIME=30.f;
bool valid_wav=true;
int check_for_invalid_wav_formats(const std::string&,std::string&){return valid_wav?0:1;}
struct LLResourceUploadInfo {
    using ptr_t=std::shared_ptr<LLResourceUploadInfo>;
    virtual ~LLResourceUploadInfo()=default;
    virtual LLUUID finishUpload(LLSD&){return {"item"};}
    virtual bool failedUpload(LLSD&,std::string&){return false;}
};
struct LLNewFileResourceUploadInfo:LLResourceUploadInfo {
    int cost;
    LLNewFileResourceUploadInfo(std::string,std::string,std::string,int,LLFolderType::EType,
        LLInventoryType::EType,int,int,int,int c):cost(c){}
    LLUUID finishUpload(LLSD&)override {balance.balance-=cost;return {"item"};}
};
struct LLViewerAssetUpload {
    static std::vector<LLResourceUploadInfo::ptr_t> queue;
    static void EnqueueInventoryUpload(const std::string&,const LLResourceUploadInfo::ptr_t& p){queue.push_back(p);}
};
std::vector<LLResourceUploadInfo::ptr_t> LLViewerAssetUpload::queue;
struct LLFilePicker{enum ELoadFilter{FFLOAD_ALL};enum ESaveFilter{FFSAVE_ALL};};
struct LLFilePickerReplyThread {
    static std::function<void(const std::vector<std::string>&,LLFilePicker::ELoadFilter,LLFilePicker::ESaveFilter)> picked;
    template<class F>static void startPicker(F f,LLFilePicker::ELoadFilter,bool){picked=f;}
};
std::function<void(const std::vector<std::string>&,LLFilePicker::ELoadFilter,LLFilePicker::ESaveFilter)> LLFilePickerReplyThread::picked;
// Production translation unit is inserted here by tasia_feature_restore_test.py.
