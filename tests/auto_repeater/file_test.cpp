#include "host_file_shim.hpp"
#include "../../firmware/application/repeater_sub_codec.hpp"
#include <cassert>
#include <vector>
#include <iterator>
namespace fs = std::filesystem;
using auto_repeater::FileJob;
void write(const fs::path& p, const std::vector<complex16_t>& v) {
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char*>(v.data()), v.size() * 4);
}
std::vector<complex16_t> read(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::vector<complex16_t> v(fs::file_size(p) / 4);
    f.read(reinterpret_cast<char*>(v.data()), v.size() * 4);
    return v;
}
struct Source {
    std::string s;
    size_t i{0};
    bool get(char& c) {
        if (i == s.size()) return false;
        c = s[i++];
        return true;
    }
};
struct Sink {
    uint64_t us{0};
    size_t edges{0};
    bool pulse(bool, unsigned n) {
        us += n;
        ++edges;
        return true;
    }
};
int main(int argc, char** argv) {
    assert(argc == 2);
    fs::path dir = argv[1];
    fs::create_directories(dir);
    const auto sub = dir / "raw.SUB", iq = dir / "import.C16";
    {
        std::ofstream f(sub);
        f << "Filetype: Flipper SubGhz RAW File\nVersion: 1\nFrequency: 433920000\nPreset: FuriHalSubGhzPresetOok650Async\nProtocol: RAW\nRAW_Data: 300 -900 900 -300\n";
    }
    FileJob j;
    j.input = sub;
    j.output = iq;
    auto_repeater::process_repeater_files(j);
    assert(j.ok);
    auto v = read(iq);
    assert(v.size() == 1200);
    assert(v[0].real() == 16384);
    assert(v[150].real() == 0);
    assert(v[600].real() == 16384);
    assert(j.frequency == 433920000);
    {
        auto tmp = dir / "ring.TMP", out = dir / "ordered.C16";
        write(tmp, {{3, 0}, {4, 0}, {1, 0}, {2, 0}, {5, 0}, {6, 0}});
        FileJob k;
        k.kind = FileJob::FinishRing;
        k.input = tmp;
        k.output = out;
        k.prefix = 16;
        k.end = 8;
        auto_repeater::process_repeater_files(k);
        assert(k.ok);
        auto x = read(out);
        for (int i = 0; i < 6; i++) assert(x[i].real() == i + 1);
        assert(!fs::exists(tmp));
    }
    const auto ook = dir / "ook.C16";
    std::vector<complex16_t> pulses(1000, {0, 0});
    for (int n = 0; n < 8; n++) {
        pulses.insert(pulses.end(), 150, {16384, 0});
        pulses.insert(pulses.end(), 450, {0, 0});
    }
    write(ook, pulses);
    write_metadata_file(get_metadata_path(ook), {433920000, 500000});
    {
        FileJob k;
        k.kind = FileJob::ExportSUB;
        k.output = ook;
        k.export_sub = true;
        auto_repeater::process_repeater_files(k);
        assert(k.ok && !k.error);
        auto p = ook;
        p.replace_extension(".SUB");
        std::ifstream f(p);
        Source s{std::string(std::istreambuf_iterator<char>(f), {})};
        Sink sink;
        auto_repeater::SubInfo info;
        assert(!auto_repeater::parse_sub(s, sink, info));
        assert(info.frequency == 433920000);
        assert(sink.us == pulses.size() * 2);
        assert(sink.edges == 17);
    }
    {
        auto p = dir / "constant.C16";
        write(p, std::vector<complex16_t>(2000, {16384, 0}));
        write_metadata_file(get_metadata_path(p), {433920000, 500000});
        FileJob k;
        k.kind = FileJob::ExportSUB;
        k.output = p;
        k.export_sub = true;
        auto_repeater::process_repeater_files(k);
        assert(k.ok && k.error);
        p.replace_extension(".SUB");
        assert(!fs::exists(p));
    }
    {
        auto p = dir / "bad.SUB";
        std::ofstream f(p);
        f << "Filetype: Flipper SubGhz Key File\nFrequency: 433920000\nProtocol: KeeLoq\nKey: 00 11\n";
        f.close();
        FileJob k;
        k.input = p;
        k.output = dir / "bad.C16";
        auto_repeater::process_repeater_files(k);
        assert(!k.ok && !fs::exists(k.output));
    }
    std::puts("PASS: actual SUB importer/exporter, generated IQ timing, circular-buffer reorder, export round-trip, non-OOK rejection and unsupported-file cleanup");
}
