# Experimental C99 RGI core

Host-testable source module, not an installed Delphi receiver. No QNX ABI, phone negotiation or vehicle integration has been validated.

Implemented: bounded control-frame/TLV decoding, numeric and UTF-8 accessors, nested lane checks, fragment/coalesced-frame handling, session/reset/timeout callbacks, and buffer-only subscription encoders. Start and stop both include the selected component ID.

The [Identify transformer](IDENTIFY.md) prepares component 0x001E and advertised RGI message lists in a separate buffer. It rejects conflicting declarations and leaves outputs unchanged on error. Capabilities are caller-supplied experimental values, not detected hardware support.

Input must already be decrypted control frames. No USB transport, authentication, decryption, live interception, BAP translation, stock-navigation arbitration or video output is implemented. Missing fields stay missing. Callbacks borrow data and must copy retained fields, clear caches on RESET and avoid reentrancy. Unknown message families must remain available to the original dispatcher in any future integration.

## Build and test

```sh
cmake -S native/rgi -B build/rgi
cmake --build build/rgi
ctest --test-dir build/rgi --output-on-failure
```

Or compile each test directly with a C99 compiler, linking both src/*.c and adding include/ to the header path. Test checks remain active with NDEBUG. Build products are not included in the release.

Tests use synthetic bytes only: they cover malformed input, 20,000 deterministic mutations, structured bit mutations, exact field widths, signed angles, atomic failure, session isolation, timeouts and Identify preservation/idempotence. See validation.json for the actual validation environment and open integration work.

The field IDs and widths were cross-checked against [public protocol notes](https://github.com/kamgurgul/mib2q-carplay/blob/master/docs/rgd/rgd-tlv.md) and associated definitions on another platform. No upstream implementation is bundled. Strict-profile compatibility still requires real captures.
