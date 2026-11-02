# Route-guidance core

Portable C99 parsing and session logic for CarPlay Route Guidance Information.
It validates frames and TLVs, maintains atomic route state, handles timeout and
reset, and prepares the iAP2 Identify declarations used by the P0867D hook.

```sh
cmake -S native/rgi -B build/rgi
cmake --build build/rgi
ctest --test-dir build/rgi --output-on-failure
```
