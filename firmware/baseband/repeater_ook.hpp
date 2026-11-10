// SPDX-License-Identifier: GPL-2.0-or-later
// Reuse Mayhem's protocol recognizers; no key recovery or rolling-code synthesis.
#pragma once
#include <algorithm>
#pragma GCC push_options
#pragma GCC optimize("Os")
#include "fprotos/s-princeton.hpp"
#include "fprotos/s-came.hpp"
#include "fprotos/s-came_atomo.hpp"
#include "fprotos/s-came_twee.hpp"
#include "fprotos/s-faac.hpp"
#include "fprotos/s-keeloq.hpp"
#include "fprotos/s-secplus_v1.hpp"
#include "fprotos/s-secplus_v2.hpp"
#include "fprotos/s-nice_flo.hpp"
#include "fprotos/s-holtek_ht12x.hpp"
#include "portapack_shared_memory.hpp"
class RepeaterOOK {
   public:
    void configure(uint32_t sample_rate) {
        princeton = FProtoSubGhzDPrinceton{};
        came = FProtoSubGhzDCame{};
        atomo = FProtoSubGhzDCameAtomo{};
        twee = FProtoSubGhzDCameTwee{};
        faac = FProtoSubGhzDFaac{};
        keeloq = FProtoSubGhzDKeeLoq{};
        sec1 = FProtoSubGhzDSecPlusV1{};
        sec2 = FProtoSubGhzDSecPlusV2{};
        nice = FProtoSubGhzDNiceflo{};
        holtek = FProtoSubGhzDHoltekHt12x{};
        rate = sample_rate;
        enabled = rate != 0;
        low = 100;
        high = 12000;
        level = false;
        run = 0;
        for (auto* p : protocols) p->setCallback(found);
    }
    void feed(const buffer_c16_t& buffer) {
        if (!enabled) return;
        for (size_t i = 0; i < buffer.count; i++) {
            const int32_t re = buffer.p[i].real(), im = buffer.p[i].imag();
            const int32_t power = (int64_t(re) * re + int64_t(im) * im) >> 10;
            if (level)
                high += (power - high) / 8;
            else {
                low += (power - low) / 128;
                high += (low * 2 + 10 - high) / 1024;
            }
            high = std::max(high, low * 2 + 10);
            const auto threshold = (high + low) / 2, margin = std::max<int32_t>(1, threshold / 8);
            const bool next = level ? power >= threshold - margin : power >= threshold + margin;
            ++run;
            if (next != level || uint64_t(run) * 1000000 >= uint64_t(rate) * 30000) {
                const uint32_t us = uint64_t(run) * 1000000 / rate;
                if (us >= 8)
                    for (auto* p : protocols) p->feed(level, us);
                level = next;
                run = 0;
            }
        }
    }

   private:
    static void found(FProtoSubGhzDBase* p) {
        SubGhzDDataMessage m{p->sensorType, p->data_count_bit, p->decode_data};
        shared_memory.application_queue.push(m);
    }
    bool enabled{false}, level{false};
    uint32_t rate{100000}, run{0};
    int32_t low{100}, high{12000};
    FProtoSubGhzDPrinceton princeton{};
    FProtoSubGhzDCame came{};
    FProtoSubGhzDCameAtomo atomo{};
    FProtoSubGhzDCameTwee twee{};
    FProtoSubGhzDFaac faac{};
    FProtoSubGhzDKeeLoq keeloq{};
    FProtoSubGhzDSecPlusV1 sec1{};
    FProtoSubGhzDSecPlusV2 sec2{};
    FProtoSubGhzDNiceflo nice{};
    FProtoSubGhzDHoltekHt12x holtek{};
    FProtoSubGhzDBase* protocols[10]{&princeton, &came, &atomo, &twee, &faac, &keeloq, &sec1, &sec2, &nice, &holtek};
};

#pragma GCC pop_options
