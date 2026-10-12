# Memory-Safe Cosmopolitan Libc (filc-cosmopolitan)

This is a memory-safe fork of [Cosmopolitan Libc](https://github.com/jart/cosmopolitan) designed specifically for the `fil-c` compiler.

It integrates `fil-c`'s bounds-checking and 128-bit fat pointers directly into Cosmopolitan's lowest-level syscalls and memory allocators (e.g. `filc_malloc.c`, `filc_str.c`, `filc_sigaction.c`). 

By compiling this repository with `fil-c`, you can produce **Memory-Safe Actually Portable Executables (APEs)** that run natively on Windows, Linux, Mac, and BSD while being completely immune to standard C memory vulnerabilities like buffer overflows.

## License
This fork respects Cosmopolitan's original ISC License. See `LICENSE` for details.
