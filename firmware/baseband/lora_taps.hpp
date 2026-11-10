// From Alexey Verhogladov mesh-test-1; GPL-2.0-or-later
#pragma once
#include "dsp_fir_taps.hpp"
// LoRa BW125: second decimation stage, 250 kHz -> 125 kHz. Cutoff at fs_in/4, so the
// +/-62.5 kHz a BW125 chirp occupies is down only 6 dB at its very edges while what
// would alias into it is 16 dB down at 0.30 and 49 dB at 0.40. Designed here rather
// than borrowed: the existing 16-tap tables pass only 0.13 of the input rate, which
// would put the whole chirp in the transition band.
static constexpr fir_taps_real<16> taps_125k_decim_2 = {
    .low_frequency_normalized = -0.25f,
    .high_frequency_normalized = 0.25f,
    .transition_normalized = 0.10f,
    .taps = {{
        -79,
        -136,
        312,
        654,
        -1244,
        -2280,
        4501,
        14655,
        14657,
        4501,
        -2280,
        -1244,
        654,
        312,
        -136,
        -79,
    }},
};

