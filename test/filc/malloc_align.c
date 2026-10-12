// Checks the filc_malloc.c fixes:
//  - pvalloc() rounds the usable size up to a whole page (pvalloc(0) -> 1 page)
//  - pvalloc() rejects sizes that would overflow when rounded
//  - aligned_alloc()/memalign() accept small power-of-two alignments (1,2,4)
//  - aligned_alloc() does NOT reject a size that isn't a multiple of align
//  - invalid alignments (0, non-power-of-two) still fail with EINVAL
#include <errno.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(cond)                                         \
  do {                                                      \
    if (cond) {                                             \
      printf("PASS  %s\n", #cond);                          \
    } else {                                                \
      printf("FAIL  %s  (line %d)\n", #cond, __LINE__);     \
      failures++;                                           \
    }                                                       \
  } while (0)

int main(void) {
  char *p = pvalloc(10);
  CHECK(p != NULL);
  CHECK(((uintptr_t)p & 4095) == 0);
  CHECK(malloc_usable_size(p) == 4096);
  memset(p, 1, 4096);  // would trap before the fix (bounds were 10 bytes)
  CHECK(p[4095] == 1);

  char *z = pvalloc(0);
  CHECK(z != NULL && malloc_usable_size(z) == 4096);

  char *q = pvalloc(4097);
  CHECK(q != NULL && malloc_usable_size(q) == 8192);

  errno = 0;
  CHECK(pvalloc((size_t)-1) == NULL && errno == ENOMEM);

  CHECK(aligned_alloc(1, 16) != NULL);
  CHECK(aligned_alloc(4, 16) != NULL);
  CHECK(memalign(2, 16) != NULL);

  char *a = aligned_alloc(64, 100);  // 100 is not a multiple of 64: allowed
  CHECK(a != NULL && ((uintptr_t)a & 63) == 0);
  CHECK(malloc_usable_size(a) >= 100);

  errno = 0;
  CHECK(aligned_alloc(0, 16) == NULL && errno == EINVAL);
  errno = 0;
  CHECK(aligned_alloc(24, 48) == NULL && errno == EINVAL);
  errno = 0;
  CHECK(memalign(3, 16) == NULL && errno == EINVAL);

  void *r = NULL;
  CHECK(posix_memalign(&r, 4, 16) == EINVAL);  // still needs >= sizeof(void*)
  CHECK(posix_memalign(&r, 16, 16) == 0 && r != NULL);

  printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures,
         failures == 1 ? "" : "s");
  return failures != 0;
}
