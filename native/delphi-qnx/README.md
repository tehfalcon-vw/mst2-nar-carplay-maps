# P0867D QNX payloads

Freestanding ARMv7 preload libraries for the exact P0867D CarPlay, MediaHandler,
mm-ipod, and NavigationServer processes. All runtime hooks validate the target
before activation and fail closed on a mismatch.

Build and verify on Linux with Clang and LLD:

```sh
sh native/delphi-qnx/build.sh
sh native/delphi-qnx/verify.sh
```

The minimal `libc.so.3` in `stubs/` is a link-only import surface. It is not
shipped or loaded on the unit.
