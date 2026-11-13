# Channel-key fix and receive diagnostics (ar05)

Custom channels with empty keys now use a zero-length PSK and the correct channel hash. The previous implementation generated a synthetic AES key despite displaying 'open'.

Custom channel keys support 128-bit and 256-bit raw hex or padded Base64. Typed entry allows 64 characters. Import key reads a key-only .TXT file. Existing custom passphrases retain their prior derivation. Primary-channel default-key behavior is unchanged.

Receive decode failures are reported on the first failure and every group of eight. Channel-hash diagnostics remain separate.

The ARM build and layout, runtime-memory, installation and update-archive checks passed. The corrected Flash Utility archive uses USTAR headers with explicit directories. Craig flashed it successfully. The PortaPack imported a 256-bit key and exchanged text in both directions with the LILYGO on the test secondary channel. Repeated delivery still needs investigation.

See AR05-RELEASE-NOTES.md for test scope and remaining validation. Download the complete SD-card and Windows flashing package from the ar05 GitHub release.
