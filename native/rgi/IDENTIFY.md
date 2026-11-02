# iAP2 Identify transformation

`dcc_identify_prepare` validates a complete plaintext iAP2 Identify message,
adds the RGI component and required message IDs, and preserves unrelated TLVs.
The caller supplies the component ID and buffer capacities.

`dcc_identify_component_payload` builds the nested RGI component payload used
by the Delphi P0867D hook.

Both functions validate and size their output before writing. Conflicting RGI
components, malformed TLVs, duplicate fields, invalid UTF-8, and insufficient
output capacity are rejected without modifying the destination buffer.

The implementation handles decrypted iAP2 control messages. USB framing,
AirPlay encryption, and transport are implemented by the platform-specific
runtime rather than this module.
