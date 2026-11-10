#pragma once
#include <filesystem>
#include <fstream>
#include <optional>
#include <atomic>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
using complex16_t = std::complex<int16_t>;
static_assert(sizeof(complex16_t) == 4);
template <class T>
struct HostResult {
    T v{};
    bool error{false};
    bool is_error() const { return error; }
    bool is_ok() const { return !error; }
    T value() const { return v; }
};
class File {
   public:
    using Size = uint64_t;
    template <class T>
    using Result = HostResult<T>;
    std::optional<int> open(const std::filesystem::path& p) {
        path = p;
        stream.open(p, std::ios::in | std::ios::out | std::ios::binary);
        return stream ? std::optional<int>{} : std::optional<int>{1};
    }
    std::optional<int> create(const std::filesystem::path& p) {
        path = p;
        stream.open(p, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        return stream ? std::optional<int>{} : std::optional<int>{1};
    }
    Result<Size> read(void* p, Size n) {
        stream.read(static_cast<char*>(p), n);
        return {uint64_t(stream.gcount()), stream.bad()};
    }
    Result<Size> write(const void* p, Size n) {
        stream.write(static_cast<const char*>(p), n);
        return {n, !bool(stream)};
    }
    Result<Size> seek(Size n) {
        stream.clear();
        stream.seekg(n);
        stream.seekp(n);
        return {n, !bool(stream)};
    }
    Size size() {
        stream.flush();
        return std::filesystem::file_size(path);
    }
    void close() { stream.close(); }
    std::optional<int> write_line(const std::string& s) {
        stream << s << '\n';
        return stream ? std::optional<int>{} : std::optional<int>{1};
    }

   private:
    std::fstream stream{};
    std::filesystem::path path{};
};
struct capture_metadata {
    int64_t center_frequency;
    uint32_t sample_rate;
};
inline std::filesystem::path get_metadata_path(std::filesystem::path p) {
    return p.replace_extension(".TXT");
}
inline std::optional<int> write_metadata_file(std::filesystem::path p, capture_metadata m) {
    std::ofstream f(p);
    f << m.center_frequency << ' ' << m.sample_rate;
    return f ? std::optional<int>{} : std::optional<int>{1};
}
inline std::optional<capture_metadata> read_metadata_file(std::filesystem::path p) {
    capture_metadata m{};
    std::ifstream f(p);
    if (f >> m.center_frequency >> m.sample_rate) return m;
    return {};
}
inline void delete_file(const std::filesystem::path& p) {
    std::filesystem::remove(p);
}
namespace auto_repeater {
struct FileJob {
    enum Kind { ImportSUB,
                ExportSUB,
                FinishRing };
    Kind kind{ImportSUB};
    std::filesystem::path input{}, output{};
    uint64_t prefix{0}, end{0};
    int64_t frequency{0};
    uint32_t threshold_percent{35};
    bool export_sub{false}, ok{false};
    const char* error{"File processing failed"};
    std::atomic<bool> cancel{false};
    std::atomic<uint32_t> percent{0};
};
void process_repeater_files(FileJob&);
}  // namespace auto_repeater
