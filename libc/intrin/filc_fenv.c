/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8     -*-│
│ vi: set noet ft=c ts=2 sts=2 sw=2 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Fil-C port of Cosmopolitan libc: software floating-point environment.        │
│                                                                              │
│ cosmo implements fenv for x86_64 in assembly (fnstcw/ldmxcsr/...).  Some of  │
│ those mnemonics are not on the Fil-Pizlonator's safe-inline-asm allowlist,   │
│ and the exception state is per-thread state anyway, so this is a plain       │
│ software implementation: flags and the rounding mode live in ordinary        │
│ variables.  The rounding mode is fixed to FE_TONEAREST, which is what the    │
│ hardware defaults to and what all of cosmo's own code assumes.               │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/runtime/fenv.h"
#include "libc/thread/tls.h"

int feclearexcept(int excepts) {
  struct CosmoTib *tib = __get_tls();
  tib->tib_fenv_flags &= ~(uint16_t)(excepts & FE_ALL_EXCEPT);
  return 0;
}

int fetestexcept(int excepts) {
  struct CosmoTib *tib = __get_tls();
  return tib->tib_fenv_flags & (uint16_t)(excepts & FE_ALL_EXCEPT);
}

int feraiseexcept(int excepts) {
  struct CosmoTib *tib = __get_tls();
  tib->tib_fenv_flags |= (uint16_t)(excepts & FE_ALL_EXCEPT);
  return 0;
}

int fesetexceptflag(const fexcept_t *fp, int mask) {
  if (!fp)
    return -1;
  struct CosmoTib *tib = __get_tls();
  uint16_t m = (uint16_t)(mask & FE_ALL_EXCEPT);
  tib->tib_fenv_flags = (tib->tib_fenv_flags & ~m) | ((uint16_t)(*fp) & m);
  return 0;
}

int fegetenv(fenv_t *envp) {
  if (!envp)
    return -1;
  struct CosmoTib *tib = __get_tls();
  uintptr_t packed = ((uintptr_t)(uint32_t)tib->tib_fenv_round << 32) |
                     ((uintptr_t)tib->tib_fenv_mask << 16) |
                     (uintptr_t)tib->tib_fenv_flags;
  *envp = (fenv_t)packed;
  return 0;
}

int fesetenv(const fenv_t *envp) {
  struct CosmoTib *tib = __get_tls();
  if (envp == FE_DFL_ENV) {
    tib->tib_fenv_flags = 0;
    tib->tib_fenv_mask = 0;
    tib->tib_fenv_round = FE_TONEAREST;
    return 0;
  }
  if (!envp)
    return -1;
  uintptr_t packed = (uintptr_t)(*envp);
  tib->tib_fenv_flags = (uint16_t)(packed & FE_ALL_EXCEPT);
  tib->tib_fenv_mask = (uint16_t)((packed >> 16) & FE_ALL_EXCEPT);
  tib->tib_fenv_round = (int32_t)(uint32_t)(packed >> 32);
  return 0;
}

int fegetround(void) {
  struct CosmoTib *tib = __get_tls();
  return tib->tib_fenv_round;
}

int __fesetround(int r) {
  struct CosmoTib *tib = __get_tls();
  tib->tib_fenv_round = r;
  return 0;
}

int feenableexcept(int excepts) {
  struct CosmoTib *tib = __get_tls();
  int old = tib->tib_fenv_mask;
  tib->tib_fenv_mask |= (uint16_t)(excepts & FE_ALL_EXCEPT);
  return old;
}

int fedisableexcept(int excepts) {
  struct CosmoTib *tib = __get_tls();
  int old = tib->tib_fenv_mask;
  tib->tib_fenv_mask &= ~(uint16_t)(excepts & FE_ALL_EXCEPT);
  return old;
}

int fegetexcept(void) {
  struct CosmoTib *tib = __get_tls();
  return tib->tib_fenv_mask;
}


