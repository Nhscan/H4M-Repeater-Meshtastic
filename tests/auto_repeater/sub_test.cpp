#include "../../firmware/application/repeater_sub_codec.hpp"
#include <string>
#include <vector>
#include <cassert>
#include <cstdio>
struct Source {
    const std::string& s;
    size_t i{0};
    bool get(char& c) {
        if (i == s.size()) return false;
        c = s[i++];
        return true;
    }
};
struct Sink {
    std::vector<int> pulses{};
    bool pulse(bool high, unsigned us) {
        pulses.push_back(high ? int(us) : -int(us));
        return true;
    }
};
const std::string header = "Filetype: Flipper SubGhz RAW File\nVersion: 1\nFrequency: 433920000\nPreset: FuriHalSubGhzPresetOok650Async\nProtocol: RAW\n";
int main() {
    using auto_repeater::SubInfo;
    {
        auto data = header + "RAW_Data: 300 -900 900 -300\nRAW_Data: 300 -10000";
        Source s{data};
        Sink out;
        SubInfo info;
        assert(!auto_repeater::parse_sub(s, out, info));
        assert(info.frequency == 433920000);
        assert((out.pulses == std::vector<int>{300, -900, 900, -300, 300, -10000}));
    }
    {
        std::string data = "Filetype: Flipper SubGhz Key File\nFrequency: 315000000\nPreset: FuriHalSubGhzPreset2FSKDev2380Async\nProtocol: BinRAW\nTE: 100\nBit_RAW: 10\nData_RAW: A5 C0\n";
        Source s{data};
        Sink out;
        SubInfo info;
        assert(!auto_repeater::parse_sub(s, out, info));
        assert(info.deviation == 2380);
        assert(out.pulses.size() == 10);
        assert(out.pulses[0] == 100 && out.pulses[1] == -100 && out.pulses[8] == 100 && out.pulses[9] == 100);
    }
    {
        auto data = header + "RAW_Data: 300 BAD -200";
        Source s{data};
        Sink out;
        SubInfo info;
        assert(auto_repeater::parse_sub(s, out, info));
    }
    {
        auto data = header;
        auto p = data.find("Protocol: RAW");
        data.replace(p, 13, "Protocol: Princeton");
        data += "Key: 00 01\n";
        Source s{data};
        Sink out;
        SubInfo info;
        assert(auto_repeater::parse_sub(s, out, info));
    }
    {
        auto data = header + "RAW_Data:";
        for (int i = 0; i < 20000; i++) data += " 100 -100";
        data += '\n';
        Source s{data};
        Sink out;
        SubInfo info;
        assert(!auto_repeater::parse_sub(s, out, info));
        assert(out.pulses.size() == 40000);
    }
    std::puts("PASS: RAW multiline/EOF, BinRAW bit count, OOK/FSK preset, bad numbers, unsupported protocol and long lines");
}
