# Point 1 — RGI Identify preparation and integration boundary

## Implemented contract

`dcc_identify_prepare` accepts a complete plaintext iAP2 control message `0x1D01`, an explicit experimental capability profile, and a distinct destination buffer. It performs validation and sizing before changing any output. It does not edit firmware, configuration, transport packets or device memory.

It adds a component `0x001E` with nine nested attributes: component ID, NUL-terminated UTF-8 name, four text capacities for road/destination/after-road/maneuver description, maneuver storage capacity, lane description capacity and lane storage capacity. Numeric attributes are big-endian u16. The component ID and capacities come from the caller: they are not inferred from the cluster and must not be treated as supported hardware capabilities.

The outgoing message-ID list receives only missing `0x5200` and `0x5203`; the incoming list receives only missing `0x5201`, `0x5202`, `0x5204`. Existing list bytes/order and unrelated top-level TLVs are preserved. Lists are limited to 128 bytes, matching the bound observed in the original driver's construction path.

The strict initial policy rejects a different existing RGI component instead of overwriting another integration. An exact canonical match is preserved. Known transport/HID/location/vehicle component IDs are checked for collisions; unknown future component namespaces remain opaque, so global uniqueness beyond the supported families is not certified. Noncanonical but potentially valid RGI declarations are intentionally not silently normalized.

## Native insertion boundary — still open

The original driver assembles the identification through `ident_info_request`, then calls its message sender with ID `0x1D01`. The transform belongs after the complete control frame has been serialized and before lower transport framing/checksum generation. It must not be called on USB fragments, encrypted data or an opaque driver packet struct cast to a byte array.

The present module does not supply that native binding. A future binding must establish the actual frame pointer, length, capacity, lifetime, ownership and error path. Internal direct ARM calls must not be assumed interceptable through LD_PRELOAD just because a symbol is globally exported. On a transform error, the original frame must remain available untouched; whether to send the original or abort that session is a separate integration policy, not implemented here.

The component identifier used in Identify must agree with the subsequent subscription and received events. One integration must own the declaration. A capability profile must reflect implemented buffers/caches, not desired future features. Current tests use explicit synthetic profiles only; they do not authorize advertising a live receiver that has not yet been implemented.

## Validation performed

The C sources compile with warnings treated as errors. Tests execute in a Wasmtime/WASI sandbox with no preopened directories or sockets, 64 MiB memory limit and bounded instruction fuel. Compilation includes undefined-behavior checks and NDEBUG; test checks are always active.

Tests cover all nine attributes, message directions, partial/missing lists, no duplicate additions, preservation of opaque and original location fields, input immutability, idempotence, conflicts, duplicates, malformed TLVs, odd numeric lists, UTF-8, overlapping buffers, output/list/frame limits, atomic rejection and single-bit mutations of a synthetic Identify frame.

The newly built Windows native test executable was blocked by Windows Application Control. Protections were not modified; the same test sources were compiled to sandboxed WebAssembly instead. No successful native Windows execution of this new Identify test is claimed.

## What closes point 1

The buffer-level implementation is ready for review. Point 1 for the real Delphi remains open until the binding is implemented, the actual serialized Identify is inspected and the phone accepts it. There are no real Delphi Identify or RGI session captures in the current test corpus. Tests on synthetic bytes do not prove negotiation, QNX ABI compatibility, or installation safety. Work on receiving and HMI integration remains separate; no device operations or publication have occurred.

Protocol reference: [public RGI field documentation](https://github.com/kamgurgul/mib2q-carplay/blob/master/docs/rgd/rgd-tlv.md) and associated numeric field definitions, consulted as a different-platform reference. No upstream implementation is bundled.

