# Open-channel fix and receive diagnostics (unreleased)

The custom-channel picker labels an empty key as `open`, but previously
`derive_channel_key` returned a synthetic AES-128 key beginning with 01.
`apply_channels` then always installed 16 key bytes. Both the channel hash
and payload encryption differed from a stock Meshtastic channel with no PSK.

Empty keys now install a zero-length PSK and hash only the channel name.
Raw 32-character hexadecimal keys and existing passphrases keep their previous
128-bit key derivation. Public Primary-channel behavior is unchanged.

The chat also reports the first router decode failure, and then each group
of eight failures. This distinguishes a demodulated frame that fails parsing
or decryption from no frame reaching the router. Existing channel-hash
diagnostics remain separate. This does not yet expose M4 preamble/header failures.

Validation: `test_mesh_channel_key` uses the actual firmware router and crypto
with a standard plaintext Data-protobuf fixture. It reproduces rejection by
the previous synthetic-key setup, verifies open-channel RX/TX, raw keys,
legacy passphrases, and removal of a key. Existing `test_mesh_router` passes.

Not yet ARM-built or flashed. These tests do not establish RF interoperability
or resolve the separate intermittent receive problem observed on hardware.
Build matching FIRMWARE/APPS/BASEBAND using docs/h4m/BUILD.txt before testing.
