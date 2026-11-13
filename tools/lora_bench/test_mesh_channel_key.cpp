// Exercise channel setup against real router/crypto code and a plaintext wire fixture.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include "../../firmware/application/apps/mesh/mesh_channel_key.hpp"
#include "../../firmware/application/apps/mesh/mesh_regions.hpp"
#include "../../firmware/application/apps/mesh/mesh_router.hpp"

using namespace meshtastic;

int main() {
    uint8_t key[16];
    const size_t key_len = derive_custom_channel_key("", 0, key);
    assert(key_len == 0);
    const uint8_t hash = channel_hash("test", key, key_len);
    assert(hash == static_cast<uint8_t>('t' ^ 'e' ^ 's' ^ 't'));
    NodeDB tx_db{}, rx_db{};
    MeshRouter tx{tx_db}, rx{rx_db};
    tx.set_local_node(0x12345678, DEFAULT_PSK, sizeof(DEFAULT_PSK));
    rx.set_local_node(0x87654321, DEFAULT_PSK, sizeof(DEFAULT_PSK));
    tx.set_channel(1, hash, key, key_len, true);
    rx.set_channel(1, hash, key, key_len, true);
    tx.set_active_channel(1);
    uint8_t frame[PKT_MAX_SIZE];
    const size_t len = tx.build_text_tx(frame, sizeof(frame), "OPEN", 4);
    assert(len > PKT_HEADER_SIZE);
    assert(PacketHeader::from_bytes(frame).channel_hash == hash);
    DecodedData data;
    assert(DecodedData::decode(frame + PKT_HEADER_SIZE, len - PKT_HEADER_SIZE, data));
    assert(data.portnum == PortNum::TEXT_MESSAGE);
    assert(data.payload_len == 4 && std::memcmp(data.payload, "OPEN", 4) == 0);
    // Standard Data protobuf: portnum=1, payload="OPEN", with no ciphertext.
    const uint8_t plaintext[] = {0x08, 0x01, 0x12, 0x04, 'O', 'P', 'E', 'N'};
    std::memcpy(frame + PKT_HEADER_SIZE, plaintext, sizeof(plaintext));
    std::string received;
    rx.set_on_packet([&](const MeshPacket& p) { received = p.text_payload(); });
    rx.on_raw_rx(frame, PKT_HEADER_SIZE + sizeof(plaintext), -50, 8.0f, 100);
    assert(received == "OPEN");
    assert(rx.counters().rx_bad == 0);

    // Reproduce the old "open" setup: its synthetic AES key changes the hash
    // and rejects a stock node's actual unencrypted frame before decryption.
    NodeDB old_db{};
    MeshRouter old_rx{old_db};
    const uint8_t old_key[16] = {1};
    old_rx.set_local_node(0x87654321, DEFAULT_PSK, sizeof(DEFAULT_PSK));
    old_rx.set_channel(1, channel_hash("test", old_key, sizeof(old_key)),
                       old_key, sizeof(old_key), true);
    old_rx.on_raw_rx(frame, PKT_HEADER_SIZE + sizeof(plaintext), -50, 8.0f, 100);
    assert(old_rx.bad_hash_count() == 1);

    // Existing raw keys, including mixed-case hexadecimal, retain all 128 bits.
    const char* raw = "00112233445566778899aAbBcCdDeEfF";
    assert(derive_custom_channel_key(raw, 32, key) == 16);
    for (size_t i = 0; i < 16; ++i) assert(key[i] == i * 0x11);
    const char* pass = "test";
    assert(derive_custom_channel_key(pass, 4, key) == 16);
    const uint8_t legacy[] = {0xea, 0x59, 0xa9, 0x0c, 0x62, 0xd1, 0x21, 0x84,
                              0xfa, 0x49, 0xb9, 0x1c, 0x72, 0xc1, 0x31, 0x94};
    assert(std::memcmp(key, legacy, sizeof(key)) == 0);
    assert(derive_custom_channel_key("", 0, key) == 0);  // removing a key reopens it
    for (uint8_t byte : key) assert(byte == 0);
    std::puts("PASS: plaintext channel RX/TX, raw keys, legacy passphrases, key removal");
}
