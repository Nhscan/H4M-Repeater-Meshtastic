// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "file.hpp"
#include "ch.h"
#include <atomic>
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
    std::atomic<bool> cancel{false}, done{false};
    std::atomic<uint32_t> percent{0};
    void (*processor)(FileJob&){nullptr};
    Thread* thread{nullptr};
    ~FileJob() {
        cancel = true;
        if (thread) {
            chThdWait(thread);
            thread = nullptr;
        }
    }
    static msg_t run(void* p) {
        auto* j = static_cast<FileJob*>(p);
        j->processor(*j);
        j->done = true;
        return 0;
    }
    bool start() {
        thread = chThdCreateFromHeap(nullptr, 6144, NORMALPRIO, run, this);
        return thread != nullptr;
    }
};
}  // namespace auto_repeater
