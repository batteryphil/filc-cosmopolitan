# Memory-Safe Cosmopolitan Libc (filc-cosmopolitan)

> **⚠️ HIGHLY EXPERIMENTAL**
> This is a standalone, highly experimental clone of Cosmopolitan Libc. It is actively being modified to integrate `fil-c` memory safety guarantees directly into the libc layer. Use at your own risk. Do not deploy to production.

This is a memory-safe fork of [Cosmopolitan Libc](https://github.com/jart/cosmopolitan) designed specifically for the `fil-c` compiler.

It integrates `fil-c`'s bounds-checking and capability-tracked pointers directly into Cosmopolitan's lowest-level syscalls and memory allocators (e.g. `filc_malloc.c`, `filc_str.c`, `filc_sigaction.c`).

Compiling this repository with `fil-c` produces **Actually Portable Executables (APEs) with Fil-C memory safety checks**. Most spatial and temporal memory errors (out-of-bounds access, use-after-free, double free) are caught at runtime and stop the process instead of corrupting memory. See *Known limitations* below for the gaps we know about.

## Tested so far

Only on **Linux x86_64**. The aarch64 build and the non-Linux APE targets (Windows, macOS, BSD) have not been tested.

`stress_test.c` and `test/filc/` cover:

| Test | Result |
|---|---|
| Stack buffer overflow | caught, process aborts |
| Heap buffer overflow | caught, process aborts |
| Use-after-free | caught, process aborts |
| Double free | caught, process aborts |
| `ioctl(SIOCGIFCONF)` with an undersized inner buffer (`test/filc/ioctl_nested_ptr.c`) | caught, process aborts (was **not** caught before the fix in `libc/calls/ioctl.c`) |
| Legitimate `SIOCGIFCONF` use (`test/filc/ioctl_ifconf_ok.c`) | works |
| `pvalloc` / `aligned_alloc` / `memalign` behaviour (`test/filc/malloc_align.c`) | 17/17 pass |

## Known limitations

- **ioctls whose argument struct contains pointers.** The Fil-C runtime bounds-checks the ioctl argument itself, but not pointers stored *inside* that struct. The kernel follows those inner pointers directly and can read or write past their bounds. `SIOCGIFCONF` is now checked in `libc/calls/ioctl.c`. Any other request that embeds pointers and goes through the default ioctl path is **not** protected.
- **`vfork()`** is not supported and aborts with an error. Use `fork()` or `posix_spawn()` without `POSIX_SPAWN_USEVFORK`.
- **String scanning functions** (`strlen`, `strcpy`, `strstr`, ...) are scalar rather than vectorized. This is needed for exact bounds, but it makes them slower than upstream Cosmopolitan.
- Some syscalls are unsupported in the cosmo flavor and abort when called (for example `mlockall`, `mkfifo`).

## License
This fork respects Cosmopolitan's original ISC License. See `LICENSE` for details.
