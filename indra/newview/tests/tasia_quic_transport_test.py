#!/usr/bin/env python3
"""Compile production circuit routing against deterministic transport doubles.

No simulator or GUI claims: this tests the actual C++ method bodies, not a
second implementation of the state machine. Windows build/live traffic follow.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
CIRCUIT = (ROOT / 'indra/llmessage/llcircuit.cpp').read_text()
HEADER = (ROOT / 'indra/llmessage/llcircuit.h').read_text()


def body(source, name):
    start = source.index(name)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


PRELUDE = r'''
#include <algorithm>
#include <arpa/inet.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
using U8=uint8_t; using U32=uint32_t; using S32=int; using F64=double;
using TPACKETID=U32; using LLHost=int;
constexpr int LL_PACKET_ID_SIZE=6;
struct Seconds {
    double n; Seconds(double v=0):n(v){} double value() const {return n;}
};
using F32Seconds=Seconds; using F64Seconds=Seconds;
Seconds operator+(Seconds a, Seconds b){return {a.n+b.n};}
double now=0;
#define LL_WARNS(x) std::cerr
#define LL_INFOS(x) std::cerr
#define LL_ENDL '\n'
struct LLQuicConnection {
    enum class State {CONNECTING, CONNECTED, FAILED, CLOSED};
    State state=State::CONNECTED; int fail_after=-1;
    std::vector<std::vector<U8>> sent;
    std::deque<std::vector<U8>> incoming;
    State getState()const{return state;}
    bool sendPacket(const U8* data,S32 n,bool){
        if(fail_after==0)return false;
        if(fail_after>0)--fail_after;
        sent.emplace_back(data,data+n);return true;
    }
    void close(){state=State::CLOSED;}
    std::string describeFailure()const{return "fixture failure";}
    bool receivePacket(std::vector<U8>& out){
        if(incoming.empty())return false;
        out=std::move(incoming.front());incoming.pop_front();return true;
    }
    bool receiveDatagram(std::vector<U8>&){return false;}
};
struct Record{Seconds mTimeout{2},mExpirationTime;};
struct LLReliablePacketParams{
    void set(LLHost,int,bool,Seconds,void*,void*,void*){}
};
struct LLCircuitData;
struct LLCircuit {
    std::map<LLHost,LLCircuitData*> mCircuitData,mUnackedCircuitMap;
    LLHost mNextQuicDrainKey=0;
    LLCircuitData* findCircuit(LLHost h){
        auto it=mCircuitData.find(h);return it==mCircuitData.end()?nullptr:it->second;
    }
    void serviceQuic();
    bool drainQuicPacket(U8*,S32,S32&,LLHost&);
};
struct PacketRing{
    std::vector<std::vector<U8>> sent;
    bool sendPacket(int,const char* p,int n,int){
        sent.emplace_back(p,p+n);return true;
    }
};
struct LLMessageSystem {
    PacketRing mPacketRing; int mSocket=0; LLCircuit mCircuitInfo;
    static Seconds getMessageTimeSeconds(bool=true){return {now};}
} system_instance;
LLMessageSystem* gMessageSystem=&system_instance;
struct LLCircuitData {
    LLHost mHost=1;
    std::map<U32,std::shared_ptr<Record>> mUnackedPackets;
    std::vector<U32> acked;
    void ackReliablePacket(U32 id){acked.push_back(id);mUnackedPackets.erase(id);}
    void addReliablePacket(int,U8* data,int,LLReliablePacketParams*){
        U32 id;std::memcpy(&id,data+1,4);mUnackedPackets[ntohl(id)]=std::make_shared<Record>();
    }
    void startQuic(std::shared_ptr<LLQuicConnection>);
    bool isQuic() const { return static_cast<bool>(mQuicConnection); }
    bool isQuicReady() const { return isQuic() && mQuicReady; }
    bool sendQuicPacket(const U8*,S32,bool,bool);
    void markQuicReady();void fallbackQuic();void serviceQuic();
    bool receiveQuicPacket(std::vector<U8>&);
'''

FIXTURE = r'''
std::vector<U8> packet(U32 id){
    std::vector<U8> p(16,42);U32 net=htonl(id);std::memcpy(p.data()+1,&net,4);return p;
}
void send(LLCircuitData& cd,U32 id,bool boot=false){
    auto p=packet(id);cd.mUnackedPackets[id]=std::make_shared<Record>();
    assert(cd.sendQuicPacket(p.data(),p.size(),true,boot));
}
void reset(){now=0;system_instance.mPacketRing.sent.clear();system_instance.mCircuitInfo.mUnackedCircuitMap.clear();}
int main(){
    reset();
    {
        LLCircuitData cd;auto conn=std::make_shared<LLQuicConnection>();cd.startQuic(conn);
        send(cd,1,true);send(cd,2);send(cd,3);
        assert(conn->sent.size()==1 && !cd.isQuicReady());
        assert(cd.mQuicPending.size()==2 && cd.acked.empty());
        cd.markQuicReady();assert(cd.isQuicReady() && conn->sent.size()==3);
        assert(cd.acked.empty()); // callbacks deferred, not inside send/ready
        cd.serviceQuic();assert((cd.acked==std::vector<U32>{2,3,1}));
        assert(cd.mUnackedPackets.empty() && system_instance.mPacketRing.sent.empty());
        send(cd,4);cd.serviceQuic();assert(cd.acked.back()==4);
        conn->state=LLQuicConnection::State::FAILED;cd.serviceQuic();
        assert(!cd.isQuic() && !cd.isQuicReady());
        assert(system_instance.mPacketRing.sent.size()==1); // bootstrap only, NOT actions 2,3,4
        assert(cd.mUnackedPackets.count(1)); // renewed UDP reliability, without duplicate callback
    }
    reset();
    {
        LLCircuitData cd;auto conn=std::make_shared<LLQuicConnection>();cd.startQuic(conn);
        send(cd,1,true);send(cd,2);now=4.999;cd.serviceQuic();assert(cd.isQuic());
        now=5;cd.serviceQuic();assert(!cd.isQuic());
        assert(system_instance.mPacketRing.sent==std::vector<std::vector<U8>>({packet(1),packet(2)}));
        assert(cd.acked.empty() && cd.mUnackedPackets.size()==2);
        assert(cd.mUnackedPackets[1]->mExpirationTime.value()==7);
    }
    reset();
    {
        LLCircuitData cd;auto conn=std::make_shared<LLQuicConnection>();conn->fail_after=0;cd.startQuic(conn);
        send(cd,1,true);assert(!cd.isQuic());
        assert(system_instance.mPacketRing.sent.size()==1); // no double bootstrap
    }
    reset();
    {
        LLCircuitData cd;auto conn=std::make_shared<LLQuicConnection>();cd.startQuic(conn);
        send(cd,1,true);send(cd,2);send(cd,3);send(cd,4);
        conn->fail_after=1;cd.markQuicReady();assert(!cd.isQuic());cd.serviceQuic();
        assert((cd.acked==std::vector<U32>{2})); // only accepted head
        assert(system_instance.mPacketRing.sent==std::vector<std::vector<U8>>({packet(1),packet(3),packet(4)}));
    }
    reset();
    {
        LLCircuitData cd;auto conn=std::make_shared<LLQuicConnection>();cd.startQuic(conn);
        send(cd,1,true);for(U32 id=2;id<259;++id)send(cd,id);
        assert(!cd.isQuic() && cd.mQuicPending.empty());
        assert(system_instance.mPacketRing.sent.size()==258); // cap overflow loses no pending packet
    }
    reset();
    {
        LLCircuitData cd;auto conn=std::make_shared<LLQuicConnection>();conn->state=LLQuicConnection::State::CONNECTING;
        cd.startQuic(conn);send(cd,1,true);cd.markQuicReady();assert(!cd.isQuicReady());
        conn->state=LLQuicConnection::State::CONNECTED;cd.markQuicReady();assert(cd.isQuicReady());
    }
    reset();
    {
        LLCircuitData a,b;auto ca=std::make_shared<LLQuicConnection>(),cb=std::make_shared<LLQuicConnection>();
        a.startQuic(ca);b.startQuic(cb);ca->incoming={packet(1),packet(2)};cb->incoming={packet(3)};
        LLCircuit circuits;circuits.mCircuitData={{1,&a},{2,&b}};
        U8 dest[64];int n=0,host=0;
        assert(circuits.drainQuicPacket(dest,64,n,host)&&host==1&&n==16);
        assert(circuits.drainQuicPacket(dest,64,n,host)&&host==2);
        assert(circuits.drainQuicPacket(dest,64,n,host)&&host==1);
        ca->incoming={{1,2}};assert(!circuits.drainQuicPacket(dest,64,n,host));assert(!a.isQuic());
    }
    assert(tasiaTransportStatus(true,false,true,true,false)==TasiaTransportStatus::NotRouted);
    assert(tasiaTransportStatus(true,false,true,true,true)==TasiaTransportStatus::Active);
    assert(tasiaTransportStatus(true,true,true,true,true)==TasiaTransportStatus::SecondLife);
    assert(tasiaTransportStatus(false,false,true,true,true)==TasiaTransportStatus::Disconnected);
    std::cout<<"production routing: ready, timeout, failures, queue cap, fairness, status PASS\n";
}
'''


class QuicTransport(unittest.TestCase):
    def test_production_stream_framing(self):
        stream = (ROOT/'indra/llquic/llquicstream.cpp').read_text()
        header = (ROOT/'indra/llquic/llquicstream.h').read_text()
        fields = header[header.index('    RxPhase              mRxPhase'):header.rindex('};')]
        fixture = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>
struct LLQuicStream {
    enum class RxPhase { LENGTH, PAYLOAD };
    std::vector<std::vector<uint8_t>> frames;
    std::function<void(std::vector<uint8_t>)> mOnFrame=[this](auto f){frames.push_back(std::move(f));};
    void feedRx(const uint8_t*,uint32_t);
''' + fields + '\n};\n' + body(stream, 'void LLQuicStream::feedRx(') + r'''
int main(){
    LLQuicStream stream;
    const uint8_t bytes[]={0,0,0,6, 1,2,3,4,5,6, 0,0,0,7, 7,8,9,10,11,12,13};
    for(auto byte:bytes)stream.feedRx(&byte,1);
    assert(stream.frames.size()==2 && stream.frames[0].size()==6 && stream.frames[1].size()==7);
    const uint8_t invalid[]={255,255,255,255};stream.feedRx(invalid,4);
    assert(stream.mRxInvalid && stream.frames.size()==3 && stream.frames.back().empty());
    assert(stream.mRxPayload.capacity()<65536); // huge length never reserved
    stream.feedRx(bytes,sizeof(bytes));assert(stream.frames.size()==3); // poisoned frame cannot restart
    LLQuicStream empty;
    const uint8_t keepalive_and_packet[]={0,0,0,0, 0,0,0,6, 9,8,7,6,5,4, 0,0,0,0};
    for(auto byte:keepalive_and_packet)empty.feedRx(&byte,1);
    assert(!empty.mRxInvalid && empty.frames.size()==1 && empty.frames[0].size()==6);
    assert(empty.mRxLengthBytesRead==0 && empty.mRxPhase==LLQuicStream::RxPhase::LENGTH);
    LLQuicStream isolated;
    const uint8_t standalone_keepalive[]={0,0,0,0};
    isolated.feedRx(standalone_keepalive,sizeof(standalone_keepalive));
    assert(!isolated.mRxInvalid && isolated.frames.empty());
    assert(isolated.mRxLengthBytesRead==0 && isolated.mRxPhase==LLQuicStream::RxPhase::LENGTH);
    const uint8_t following_packet[]={0,0,0,6, 1,2,3,4,5,6};
    isolated.feedRx(following_packet,sizeof(following_packet));
    assert(!isolated.mRxInvalid && isolated.frames.size()==1 && isolated.frames[0].size()==6);
    LLQuicStream large;const uint8_t maximum[]={0,0,255,255};large.feedRx(maximum,4);
    assert(!large.mRxInvalid && large.mRxExpectedLength==65535);
}
'''
        with tempfile.TemporaryDirectory(dir='/tmp/opencode', prefix='quic-framing-') as tmp:
            cpp, exe = Path(tmp)/'test.cpp', Path(tmp)/'test'
            cpp.write_text(fixture)
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', str(cpp), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_compiled_production_routing(self):
        fields = HEADER[HEADER.index('    struct QuicPacket'):HEADER.index('\n};', HEADER.index('    struct QuicPacket'))]
        implementation = CIRCUIT[CIRCUIT.index('namespace\n{\nTPACKETID quic_packet_id'):CIRCUIT.index('LLCircuitData::~LLCircuitData()')]
        status = (ROOT / 'indra/newview/lltasia_transport_status.h').read_text().replace('#pragma once', '')
        source = PRELUDE + fields + '\n};\n' + implementation + status + FIXTURE
        with tempfile.TemporaryDirectory(dir='/tmp/opencode', prefix='quic-routing-') as tmp:
            cpp, exe = Path(tmp)/'test.cpp', Path(tmp)/'test'
            cpp.write_text(source)
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                            str(cpp), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_compiled_production_quic_connection_retries(self):
        message = (ROOT/'indra/llmessage/message.cpp').read_text()
        implementation = body(message, 'bool LLMessageSystem::enableQuicCircuit(')
        fixture = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>
using S32=int; using U16=uint16_t;
#define LL_WARNS(x) std::cerr
#define LL_INFOS(x) std::cerr
#define LL_ENDL '\n'
struct LLHost {
    int id=0;
    bool isOk() const { return id!=0; }
    friend std::ostream& operator<<(std::ostream& out,const LLHost& host){return out<<host.id;}
};
struct Config {};
std::vector<bool> outcomes;
int attempts_made=0;
struct LLQuicConnection {
    explicit LLQuicConnection(std::shared_ptr<Config>) {}
    bool connect(const std::string&,U16){
        int n=attempts_made++;
        return n<static_cast<int>(outcomes.size()) && outcomes[n];
    }
    std::string describeFailure() const { return "fixture failure"; }
};
struct LLCircuitData {
    std::shared_ptr<LLQuicConnection> connection;
    int packets_out=0;
    bool isQuic() const { return static_cast<bool>(connection); }
    int getPacketsOut() const { return packets_out; }
    void startQuic(std::shared_ptr<LLQuicConnection> value){connection=std::move(value);}
};
struct LLCircuit {
    std::map<int,std::unique_ptr<LLCircuitData>> circuits;
    LLCircuitData* findCircuit(const LLHost& host){
        auto i=circuits.find(host.id);return i==circuits.end()?nullptr:i->second.get();
    }
};
struct LLQuicGlobal {
    static LLQuicGlobal& instance(){static LLQuicGlobal value;return value;}
    bool isInitialized() const {return true;}
    std::shared_ptr<Config> getClientConfiguration(){return std::make_shared<Config>();}
};
struct LLMessageSystem {
    LLCircuit mCircuitInfo;
    void enableCircuit(const LLHost& host,bool){
        if(!mCircuitInfo.findCircuit(host))mCircuitInfo.circuits[host.id]=std::make_unique<LLCircuitData>();
    }
    bool enableQuicCircuit(const LLHost&,const std::string&,U16,bool,S32);
};
''' + implementation + r'''
int main(){
    LLMessageSystem message;LLHost host{1};
    outcomes={false,false,true};attempts_made=0;
    assert(message.enableQuicCircuit(host,"sim.example",9001,true,3));
    assert(attempts_made==3 && message.mCircuitInfo.findCircuit(host)->isQuic());

    LLMessageSystem failed;host.id=2;outcomes={false,false,false};attempts_made=0;
    assert(!failed.enableQuicCircuit(host,"sim.example",9001,true,2));
    assert(attempts_made==2 && !failed.mCircuitInfo.findCircuit(host));

    LLMessageSystem clamped;host.id=3;outcomes.assign(12,false);attempts_made=0;
    assert(!clamped.enableQuicCircuit(host,"sim.example",9001,true,99));
    assert(attempts_made==10);

    LLMessageSystem minimum;host.id=4;outcomes={false,true};attempts_made=0;
    assert(!minimum.enableQuicCircuit(host,"sim.example",9001,true,0));
    assert(attempts_made==1);
    std::cout<<"production QUIC retries: success, exhaustion, max/min clamp PASS\n";
}
'''
        with tempfile.TemporaryDirectory(dir='/tmp/opencode', prefix='quic-retry-') as tmp:
            cpp, exe = Path(tmp)/'test.cpp', Path(tmp)/'test'
            cpp.write_text(fixture)
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                            str(cpp), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_negotiation_and_ready_guards(self):
        helper = (ROOT / 'indra/newview/lltasia_quic.h').read_text()
        self.assertIn('isInOpenSim()', helper)
        self.assertIn('getBOOL("TasiaQuicEnabled")', helper)
        self.assertIn('getS32("TasiaQuicConnectionRetries")', helper)
        self.assertIn('port > 65535', helper)
        settings = ET.parse(ROOT/'indra/newview/app_settings/settings.xml').getroot().find('map')
        setting_pairs = list(settings)
        retry_index = next(i for i, node in enumerate(setting_pairs)
                           if node.tag == 'key' and node.text == 'TasiaQuicConnectionRetries')
        retry_setting = setting_pairs[retry_index + 1]
        fields = list(retry_setting)
        field_map = {fields[i].text: fields[i + 1]
                     for i in range(len(fields) - 1) if fields[i].tag == 'key'}
        self.assertEqual(field_map['Type'].text, 'S32')
        self.assertEqual(field_map['Value'].text, '1')
        for path, fragment in [('llstartup.cpp', 'tasiaEnableAdvertisedQuic'),
                               ('llworld.cpp', 'tasiaEnableEventQuic'),
                               ('llviewermessage.cpp', '"RegionData"')]:
            self.assertIn(fragment, (ROOT/'indra/newview'/path).read_text())
        viewermessage = (ROOT/'indra/newview/llviewermessage.cpp').read_text()
        for name in ('void process_teleport_finish(', 'void process_crossed_region('):
            handler = body(viewermessage, name)
            self.assertLess(handler.index('tasiaEnableEventQuic'),
                            handler.index('_PREHASH_UseCircuitCode'))
            self.assertLess(handler.index('_PREHASH_UseCircuitCode'),
                            handler.index('send_complete_agent_movement'))
        enable_sim = body((ROOT/'indra/newview/llworld.cpp').read_text(),
                          'void process_enable_simulator(')
        self.assertLess(enable_sim.index('tasiaEnableEventQuic'),
                        enable_sim.index('_PREHASH_UseCircuitCode'))
        message = (ROOT/'indra/llmessage/message.cpp').read_text()
        self.assertIn('!mReceivedViaQuic || host != mLastSender', body(message, 'bool LLMessageSystem::markQuicReady('))
        enable = body(message, 'bool LLMessageSystem::enableQuicCircuit(')
        self.assertIn('getPacketsOut() > 0', enable)
        self.assertIn('retries > 10 ? 10 : retries', enable)
        self.assertIn('attempt <= attempts', enable)
        self.assertIn('candidate->connect(endpoint, port)', enable)
        global_source = (ROOT/'indra/llquic/llquicglobal.cpp').read_text()
        self.assertIn('QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION', body(global_source, 'if (!verify_cert)'))
        connection = (ROOT/'indra/llquic/llquicconnection.cpp').read_text()
        final = connection[connection.index('const bool  can_free'):connection.index('if (can_free')]
        self.assertNotIn('QUIC_DATAGRAM_SEND_SENT', final)

    def test_localized_status_xml(self):
        for language in ('en', 'pl'):
            panel = ET.parse(ROOT/f'indra/newview/skins/default/xui/{language}/panel_status_bar.xml')
            names = {node.get('name') for node in panel.getroot().findall('panel.string')}
            self.assertIn('quic_active', names)

    def test_localized_quic_preferences_xml(self):
        expected = {
            'en': 'If QUIC is unavailable or fails, the viewer automatically resumes over LLUDP when the grid permits it.',
            'pl': 'Jeśli QUIC jest niedostępny lub zawiedzie, viewer automatycznie wraca do LLUDP, gdy grid na to pozwala.',
        }
        for language, description in expected.items():
            panel = ET.parse(ROOT/f'indra/newview/skins/default/xui/{language}/panel_preferences_tasia.xml')
            root = panel.getroot()
            self.assertFalse(any(node.get('name') == 'TasiaQuicFallbackLLUDP' for node in root.iter()))
            self.assertIn(description, [node.text.strip() for node in root.iter('text') if node.text])
            self.assertTrue(any(node.get('name') == 'TasiaQuicConnectionRetries' for node in root.iter()))


if __name__ == '__main__':
    unittest.main(verbosity=2)
