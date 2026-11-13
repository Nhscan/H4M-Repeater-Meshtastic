/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __MESH_CHANNEL_KEY_H__
#define __MESH_CHANNEL_KEY_H__
#include "mesh_crypto.hpp"
#include <cstddef>
#include <cstdint>
namespace meshtastic {
inline int channel_key_hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
// Decode standard padded Base64 PSKs and raw hexadecimal AES keys.
inline size_t parse_channel_psk(const char* text, size_t len, uint8_t* out) {
    if (len == 32 || len == 64) {
        for (size_t i = 0; i < len; ++i)
            if (channel_key_hex_digit(text[i]) < 0) return 0;
        for (size_t i = 0; i < len / 2; ++i)
            out[i] = (channel_key_hex_digit(text[2*i]) << 4) | channel_key_hex_digit(text[2*i+1]);
        return len / 2;
    }
    if (len != 4 && len != 24 && len != 44) return 0;
    const size_t padding = len == 44 ? 1 : 2;
    unsigned bits = 0, acc = 0;
    uint8_t bytes[32];
    size_t n = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = text[i];
        if (i >= len - padding) {
            if (c != '=') return 0;
            continue;
        }
        int v = c >= 'A' && c <= 'Z' ? c-'A' : c >= 'a' && c <= 'z' ? c-'a'+26 :
                c >= '0' && c <= '9' ? c-'0'+52 : c=='+' ? 62 : c=='/' ? 63 : -1;
        if (v < 0) return 0;
        acc = (acc << 6) | v;
        bits += 6;
        if (bits >= 8) { bits -= 8; bytes[n++] = acc >> bits; }
    }
    if (acc & ((1u << bits)-1)) return 0;
    if (n == 1) {
        if (!bytes[0]) return 0;
        for (size_t i=0; i<16; ++i) out[i]=DEFAULT_PSK[i];
        out[15] += bytes[0]-1;
        return 16;
    }
    for (size_t i=0; i<n; ++i) out[i]=bytes[i];
    return n;
}
// Empty means no encryption. Preserve the existing raw-key/passphrase formats.
inline size_t derive_custom_channel_key(const char* text, size_t len, uint8_t out[16]) {
    for (size_t i = 0; i < 16; ++i) out[i] = 0;
    if (!len) return 0;
    bool hex = len == 32;
    for (size_t i = 0; hex && i < len; ++i)
        if (channel_key_hex_digit(text[i]) < 0) hex = false;
    if (hex) {
        for (size_t i = 0; i < 16; ++i)
            out[i] = static_cast<uint8_t>((channel_key_hex_digit(text[2 * i]) << 4) |
                                           channel_key_hex_digit(text[2 * i + 1]));
    } else {
        for (size_t i = 0; i < 16; ++i)
            out[i] = static_cast<uint8_t>(text[i % len] ^ (0x9E * (i + 1)));
    }
    return 16;
}
}  // namespace meshtastic
#endif
