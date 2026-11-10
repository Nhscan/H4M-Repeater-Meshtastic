#include <cstdint>
#include <complex>
#include <vector>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
using complex16_t=std::complex<int16_t>;
struct buffer_c16_t {complex16_t* p;size_t count;};
#include "../../firmware/baseband/repeater_ook.hpp"
int main(){
    RepeaterOOK decoder;decoder.configure(100000);
    auto pulse=[&](bool high,unsigned us){std::vector<complex16_t> v(us/10,{int16_t(high?20000:0),0});buffer_c16_t b{v.data(),v.size()};decoder.feed(b);};
    pulse(false,20000);
    for(int repeat=0;repeat<3;repeat++){
        pulse(true,390);pulse(false,14040);
        for(int bit=23;bit>=0;bit--){const bool one=(0x123456>>bit)&1;pulse(true,one?1170:390);pulse(false,one?390:1170);}
    }
    pulse(true,390);pulse(false,40000);
    bool found=false;for(const auto& p:shared_memory.application_queue.packets)if(p.sensorType==FPS_PRINCETON&&p.bits==24&&p.data==0x123456)found=true;
    assert(found);
    decoder.configure(0);shared_memory.application_queue.packets.clear();pulse(true,390);pulse(false,40000);assert(shared_memory.application_queue.packets.empty());
    std::puts("PASS: actual IQ envelope slicer + Princeton decoder recognized a known 24-bit waveform; decoding disabled in raw mode");
}
