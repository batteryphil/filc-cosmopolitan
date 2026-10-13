# Memory-Safe Cosmopolitan Libc (filc-cosmopolitan)

> **⚠️ HIGHLY EXPERIMENTAL**
> This is a standalone, experimental fork of [Cosmopolitan Libc](https://github.com/jart/cosmopolitan) tailored specifically for the [Fil-C compiler](https://github.com/batteryphil/fil-c). It integrates capability-based memory safety directly into Cosmopolitan's lowest-level syscalls and memory management layer. Use at your own risk.

This project enables building **Memory-Safe Actually Portable Executables (APEs)**. Programs compiled with this toolchain are emitted as single polyglot binaries that run natively on Linux, FreeBSD, NetBSD, and Windows while enforcing spatial and temporal memory safety via 128-bit fat pointers and a concurrent garbage collector.

---

## How It Works

1. **Pizlonated Libc Layer (`filc-cosmopolitan`)**: A curated subset of Cosmopolitan compiled with Fil-C's Clang frontend (`filcc`). Every pointer is capability-checked with exact lower and upper bounds.
2. **Runtime Translation Layer (`libpizlo` / `libpas`)**: Intercepts `zsys_*` system calls and provides dynamic flag and constant mapping (translating Linux socket, clock, and mmap flags to BSD equivalents).
3. **Cosmopolitan OS Abstraction (`libyolocosmo`)**: The un-pizlonated Cosmopolitan foundation that interfaces directly with underlying host OS kernels (Linux, FreeBSD, NetBSD, Windows NT).
4. **Binary Packaging**: Post-link `apelink` packs the binary into a dual MBR/PE/ELF format that executes natively across operating systems.

---

## Compiling Applications

To build a memory-safe Actually Portable Executable using the Fil-C compiler:

```bash
# Compile C source into an APE binary (.com)
filcc --filc-cosmo --filc-ape -O2 main.c -o main.com

# Run natively on Linux, BSD, or Windows
./main.com
```

---

## Multi-OS Test Matrix

The test payload (`test/filc/` and `stress_test.c`) has been validated across bare-metal Linux and automated headless QEMU virtual machines (`test/vm/`):

| Operating System | POSIX Smoke (`portable_smoke`) | Memory Attacks (Stack/Heap/UAF/DoubleFree) | Malloc Conformance (`malloc_align`) | `ioctl(SIOCGIFCONF)` Inner Bounds Check |
|---|---|---|---|---|
| **Ubuntu 24.04 (Host)** | 15/15 PASS | Caught (Process Aborts) | 17/17 PASS | Caught (Trap on Overwrite) |
| **Debian 12** | 15/15 PASS | Caught (Process Aborts) | 17/17 PASS | Caught (Trap on Overwrite) |
| **Fedora 44** | 15/15 PASS | Caught (Process Aborts) | 17/17 PASS | Caught (Trap on Overwrite) |
| **Alpine 3.23 (musl)** | 15/15 PASS | Caught (Process Aborts) | 17/17 PASS | Caught (Trap on Overwrite) |
| **CentOS 7** | 15/15 PASS | Caught (Process Aborts) | 17/17 PASS | Caught (Trap on Overwrite) |
| **FreeBSD 14.5** | 15/15 PASS | Caught (Process Aborts) | 17/17 PASS | Caught (Trap on Overwrite) |
| **NetBSD 10.2** | 13/15 PASS | Caught (Process Aborts) | 17/17 PASS | Caught (Trap on Overwrite) |
| **Windows Server 2022** | PASS (Native PE) | Caught (Process Aborts) | 17/17 PASS | Caught (Trap on Overwrite) |

*All 4 core memory safety exploits (`stress_test.c`: stack buffer overflow, heap buffer overflow, use-after-free, double-free) are deterministically intercepted across all tested operating systems.*

---

## Known Limitations

- **Kernel writes via nested pointers**: Fil-C automatically bounds-checks direct syscall argument buffers, but cannot intercept kernel writes through pointers *nested inside* structs. `SIOCGIFCONF` is protected with explicit `zcheck()` in `libc/calls/ioctl.c`. Other ioctls embedding inner pointers require similar explicit guards.
- **Windows `mmap`**: Anonymous `mmap` is supported via memory zeroing. File-backed `mmap` on Windows is not supported due to Windows NT virtual memory constraints.
- **OpenBSD 7.5+ W^X and `pinsyscalls`**: OpenBSD enforces strict kernel-level system call origin verification (`pinsyscalls`). APE binaries running on OpenBSD should be assimilated (`./binary.com --assimilate`) to produce native OpenBSD ELF headers.
- **Scalar string scanning**: Vectorized string routines (`strlen`, `strcpy`, `strstr`) are replaced with exact-bounds scalar loops to prevent false-positive capability traps on aligned over-reads.

---

## License

This fork respects Cosmopolitan's original ISC License. See `LICENSE` for details.
