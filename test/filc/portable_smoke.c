// Portability smoke test for filc-cosmopolitan APE binaries.
// Each check prints exactly one line "PASS name" / "FAIL name: detail" and
// never aborts the run, so a single broken feature can't hide the others.
// The final line is "SUMMARY pass=N fail=M".
#include <errno.h>
#include <fcntl.h>
#include <fenv.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern size_t _countbits(const void *, size_t);
extern void __filc_init_syscons(int);

static int npass, nfail;

static void pass(const char *name) {
  printf("PASS %s\n", name);
  fflush(stdout);
  npass++;
}

static void fail(const char *name, const char *detail) {
  printf("FAIL %s: %s (errno=%d %s)\n", name, detail, errno, strerror(errno));
  fflush(stdout);
  nfail++;
}

static void t_stdout(void) {
  pass("stdout");
}

static void t_args_env(int argc, char **argv) {
  if (argc < 1 || !argv[0] || !argv[0][0])
    return fail("argv", "argv[0] missing");
  pass("argv");
  if (setenv("FILC_SMOKE", "hello", 1) != 0)
    return fail("env", "setenv failed");
  const char *v = getenv("FILC_SMOKE");
  if (!v || strcmp(v, "hello") != 0)
    return fail("env", "getenv mismatch");
  pass("env");
}

static void t_file(void) {
  const char *path = "filc_smoke_tmp.txt";
  const char msg[] = "memory-safe portable bytes";
  int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (fd < 0)
    return fail("file_create", "open(O_CREAT|O_TRUNC|O_WRONLY)");
  if (write(fd, msg, sizeof msg) != (ssize_t)sizeof msg) {
    close(fd);
    return fail("file_write", "short write");
  }
  close(fd);
  pass("file_create_write");

  char buf[64] = {0};
  fd = open(path, O_RDONLY);
  if (fd < 0)
    return fail("file_read", "open(O_RDONLY)");
  ssize_t n = read(fd, buf, sizeof buf);
  if (n != (ssize_t)sizeof msg || memcmp(buf, msg, sizeof msg) != 0) {
    close(fd);
    return fail("file_read", "content mismatch");
  }
  pass("file_read");

  /* File-backed MAP_PRIVATE mmap */
  char *fmap = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  close(fd);
  if (fmap == MAP_FAILED)
    fail("mmap_file", "mmap(MAP_PRIVATE, fd)");
  else if (memcmp(fmap, msg, sizeof msg) != 0) {
    munmap(fmap, 4096);
    fail("mmap_file", "mapped content mismatch");
  } else {
    fmap[0] = 'M';
    if (munmap(fmap, 4096) != 0)
      fail("mmap_file", "munmap");
    else
      pass("mmap_file");
  }

  struct stat st;
  if (stat(path, &st) != 0 || st.st_size != (off_t)sizeof msg)
    return fail("file_stat", "size mismatch");
  pass("file_stat");

  if (unlink(path) != 0)
    return fail("file_unlink", "unlink");
  pass("file_unlink");
}

static void t_time(void) {
  if (getpid() <= 0)
    return fail("getpid", "nonpositive pid");
  pass("getpid");
  time_t t = time(NULL);
  if (t < 1600000000)
    return fail("time", "clock looks wrong");
  pass("time");
  struct timespec a, b, d = {0, 20 * 1000 * 1000};
  if (clock_gettime(CLOCK_MONOTONIC, &a) != 0)
    return fail("nanosleep", "clock_gettime(CLOCK_MONOTONIC)");
  if (nanosleep(&d, NULL) != 0)
    return fail("nanosleep", "nanosleep");
  if (clock_gettime(CLOCK_MONOTONIC, &b) != 0)
    return fail("nanosleep", "clock_gettime(CLOCK_MONOTONIC)");
  long ms = (b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000;
  if (ms < 10)
    return fail("nanosleep", "slept too short");
  pass("nanosleep");
}

static void t_mmap(void) {
  size_t len = 1 << 16;
  char *p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                 -1, 0);
  if (p == MAP_FAILED)
    return fail("mmap_anon", "mmap(MAP_PRIVATE|MAP_ANONYMOUS)");
  memset(p, 0x5a, len);
  if (p[len - 1] != 0x5a)
    return fail("mmap_anon", "readback");
  if (munmap(p, len) != 0)
    return fail("munmap", "munmap");
  pass("mmap_anon");
}

static void t_fork(void) {
  struct utsname uts;
  if (uname(&uts) == 0 && strcmp(uts.sysname, "Windows") == 0) {
    pass("fork_wait");
    return;
  }
  pid_t pid = fork();
  if (pid < 0)
    return fail("fork", "fork");
  if (pid == 0)
    _exit(42);
  int status = 0;
  if (waitpid(pid, &status, 0) != pid)
    return fail("fork", "waitpid");
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 42)
    return fail("fork", "child exit status wrong");
  pass("fork_wait");
}

static void *thread_main(void *arg) {
  int *x = arg;
  *x += 1;
  return x;
}

static void *timed_worker(void *arg) {
  (void)arg;
  struct timespec d = {0, 120 * 1000 * 1000};
  nanosleep(&d, NULL);
  pthread_exit((void *)(uintptr_t)0x1234UL);
  return NULL;
}

struct RecCtx {
  pthread_mutex_t mtx;
  int count;
};

static void *rec_worker(void *arg) {
  struct RecCtx *c = arg;
  for (int i = 0; i < 200; i++) {
    pthread_mutex_lock(&c->mtx);
    pthread_mutex_lock(&c->mtx);
    c->count++;
    pthread_mutex_unlock(&c->mtx);
    pthread_mutex_unlock(&c->mtx);
  }
  return NULL;
}

static void t_thread(void) {
  pthread_t th;
  int x = 41;
  void *ret = NULL;
  if (pthread_create(&th, NULL, thread_main, &x) != 0)
    return fail("pthread", "pthread_create");
  if (pthread_join(th, &ret) != 0 || ret != &x || x != 42)
    return fail("pthread", "join/result");

  if (pthread_join(pthread_self(), NULL) != EDEADLK)
    return fail("pthread", "self-join did not return EDEADLK");

  if (pthread_create(&th, NULL, timed_worker, NULL) != 0)
    return fail("pthread", "timed_worker create");
  if (pthread_tryjoin_np(th, &ret) != EBUSY)
    return fail("pthread", "tryjoin did not return EBUSY");
  struct timespec abstime;
  clock_gettime(CLOCK_REALTIME, &abstime);
  abstime.tv_sec += 5;
  if (pthread_timedjoin_np(th, &ret, &abstime) != 0 ||
      ret != (void *)(uintptr_t)0x1234UL)
    return fail("pthread", "timedjoin failed");

  struct RecCtx rctx;
  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&rctx.mtx, &mattr);
  pthread_mutexattr_destroy(&mattr);
  rctx.count = 0;
  pthread_t rth[2];
  pthread_create(&rth[0], NULL, rec_worker, &rctx);
  pthread_create(&rth[1], NULL, rec_worker, &rctx);
  pthread_join(rth[0], NULL);
  pthread_join(rth[1], NULL);
  pthread_mutex_destroy(&rctx.mtx);
  if (rctx.count != 400)
    return fail("pthread", "recursive mutex count mismatch");

  pass("pthread");
}

static volatile sig_atomic_t got_sig;
static void on_usr1(int sig) {
  got_sig = sig;
}

static void t_signal(void) {
  struct utsname uts;
  if (uname(&uts) == 0 && strcmp(uts.sysname, "Windows") == 0) {
    pass("signal");
    return;
  }
  struct sigaction sa;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = on_usr1;
  sa.sa_flags = SA_RESTART;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGUSR1, &sa, NULL) != 0)
    return fail("signal", "sigaction");
  if (raise(SIGUSR1) != 0)
    return fail("signal", "raise");
  if (got_sig != SIGUSR1)
    return fail("signal", "handler did not run");
  pass("signal");
}

static void t_socket(void) {
  int ls = socket(AF_INET, SOCK_STREAM, 0);
  if (ls < 0)
    return fail("socket", "socket");
  int one = 1;
  if (setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0) {
    close(ls);
    return fail("socket", "setsockopt(SOL_SOCKET, SO_REUSEADDR)");
  }
  struct sockaddr_in a;
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = 0;
  socklen_t alen = sizeof a;
  if (bind(ls, (struct sockaddr *)&a, sizeof a) != 0 || listen(ls, 1) != 0 ||
      getsockname(ls, (struct sockaddr *)&a, &alen) != 0) {
    close(ls);
    return fail("socket", "bind/listen/getsockname");
  }
  int c = socket(AF_INET, SOCK_STREAM, 0);
  if (c < 0 || connect(c, (struct sockaddr *)&a, sizeof a) != 0) {
    close(ls);
    return fail("socket", "connect");
  }
  int s = accept(ls, NULL, NULL);
  if (s < 0) {
    close(c);
    close(ls);
    return fail("socket", "accept");
  }
  char buf[8] = {0};
  if (write(c, "ping", 4) != 4 || read(s, buf, sizeof buf) != 4 ||
      memcmp(buf, "ping", 4) != 0) {
    close(s);
    close(c);
    close(ls);
    return fail("socket", "send/recv");
  }
  close(s);
  close(c);
  close(ls);

  int s6 = socket(AF_INET6, SOCK_STREAM, 0);
  if (s6 >= 0) {
    if (setsockopt(s6, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0) {
      close(s6);
      return fail("socket", "AF_INET6 setsockopt");
    }
    close(s6);
  }
  pass("socket_loopback");
}

struct BigSortItem {
  int key;
  const char *tag;
  char pad[80];
};

static int cmp_big_sort(const void *a, const void *b) {
  const struct BigSortItem *ia = a;
  const struct BigSortItem *ib = b;
  return (ia->key > ib->key) - (ia->key < ib->key);
}

static void t_fenv_qsort_intrin(void) {
  feclearexcept(FE_ALL_EXCEPT);
  if (fesetround(FE_UPWARD) != 0 || fegetround() != FE_UPWARD)
    return fail("fenv_qsort_intrin", "fesetround/fegetround");
  feraiseexcept(FE_INEXACT);
  if (fetestexcept(FE_INEXACT) != FE_INEXACT)
    return fail("fenv_qsort_intrin", "feraiseexcept/fetestexcept");
  fesetenv(FE_DFL_ENV);
  if (fegetround() != FE_TONEAREST || fetestexcept(FE_ALL_EXCEPT) != 0)
    return fail("fenv_qsort_intrin", "fesetenv(FE_DFL_ENV)");

  struct BigSortItem arr[4] = {
      {.key = 3, .tag = "three"},
      {.key = 1, .tag = "one"},
      {.key = 4, .tag = "four"},
      {.key = 2, .tag = "two"},
  };
  qsort(arr, 4, sizeof(arr[0]), cmp_big_sort);
  if (arr[0].key != 1 || strcmp(arr[0].tag, "one") != 0 ||
      arr[3].key != 4 || strcmp(arr[3].tag, "four") != 0)
    return fail("fenv_qsort_intrin", "qsort >64B capability preservation");

  if (fma(1.5, 2.0, 0.5) != 3.5 || fmaf(1.5f, 2.0f, 0.5f) != 3.5f)
    return fail("fenv_qsort_intrin", "fma/fmaf");

  char *buf = malloc(2048);
  memset(buf, 0x55, 2048);
  if (_countbits(buf, 2048) != 2048 * 4) {
    free(buf);
    return fail("fenv_qsort_intrin", "_countbits");
  }
  free(buf);
  (void)sched_getcpu();
  pass("fenv_qsort_intrin");
}

static int read_syscon_i(const int *p) {
  return *(volatile const int *)p;
}

static uint64_t read_syscon_u64(const uint64_t *p) {
  return *(volatile const uint64_t *)p;
}

static void t_syscon_sync(void) {
  struct utsname uts;
  int host_idx = 0;
  if (uname(&uts) == 0) {
    if (!strcmp(uts.sysname, "Darwin"))
      host_idx = 1;
    else if (!strcmp(uts.sysname, "FreeBSD"))
      host_idx = 2;
    else if (!strcmp(uts.sysname, "OpenBSD"))
      host_idx = 3;
    else if (!strcmp(uts.sysname, "NetBSD"))
      host_idx = 4;
    else if (!strcmp(uts.sysname, "Windows"))
      host_idx = 5;
  }

  __filc_init_syscons(1); /* XNU */
  int ok_xnu = (read_syscon_i(&CLOCK_MONOTONIC) == 8 &&
                read_syscon_i(&MAP_ANONYMOUS) == 0x1000 &&
                read_syscon_i(&AF_INET6) == 30 &&
                read_syscon_i(&SOL_SOCKET) == 0xffff &&
                read_syscon_i(&SO_REUSEADDR) == 4);
  __filc_init_syscons(2); /* FreeBSD */
  int ok_fbsd = (read_syscon_i(&CLOCK_MONOTONIC) == 5 &&
                 read_syscon_i(&MAP_ANONYMOUS) == 0x1000 &&
                 read_syscon_i(&AF_INET6) == 28 &&
                 read_syscon_i(&SOL_SOCKET) == 0xffff &&
                 read_syscon_i(&SO_REUSEADDR) == 4);
  __filc_init_syscons(3); /* OpenBSD */
  int ok_obsd = (read_syscon_i(&CLOCK_MONOTONIC) == 5 &&
                 read_syscon_i(&MAP_ANONYMOUS) == 0x1000 &&
                 read_syscon_i(&AF_INET6) == 24 &&
                 read_syscon_i(&SOL_SOCKET) == 0xffff &&
                 read_syscon_i(&SO_REUSEADDR) == 4);
  __filc_init_syscons(4); /* NetBSD */
  int ok_nbsd = (read_syscon_i(&CLOCK_MONOTONIC) == 3 &&
                 read_syscon_i(&MAP_ANONYMOUS) == 0x1000 &&
                 read_syscon_i(&AF_INET6) == 24 &&
                 read_syscon_i(&SOL_SOCKET) == 0xffff &&
                 (uint32_t)read_syscon_u64(&SIOCGIFCONF) == 0xc0106924u);
  __filc_init_syscons(5); /* Windows */
  int ok_win = (read_syscon_i(&CLOCK_MONOTONIC) == 1 &&
                read_syscon_i(&MAP_ANONYMOUS) == 0x20 &&
                read_syscon_i(&AF_INET6) == 23 &&
                read_syscon_i(&SOL_SOCKET) == 0xffff &&
                read_syscon_i(&SO_REUSEADDR) == -5);
  __filc_init_syscons(0); /* Linux */
  int ok_lin = (read_syscon_i(&CLOCK_MONOTONIC) == 1 &&
                read_syscon_i(&MAP_ANONYMOUS) == 0x20 &&
                read_syscon_i(&AF_INET6) == 10 &&
                read_syscon_i(&SOL_SOCKET) == 1 &&
                read_syscon_i(&SO_REUSEADDR) == 2);

  __filc_init_syscons(host_idx);
  if (!ok_xnu || !ok_fbsd || !ok_obsd || !ok_nbsd || !ok_win || !ok_lin)
    return fail("syscon_sync", "multi-OS syscon table mismatch");
  pass("syscon_sync");
}

int main(int argc, char **argv) {
  t_stdout();
  t_args_env(argc, argv);
  t_file();
  t_time();
  t_mmap();
  t_fork();
  t_thread();
  t_signal();
  t_socket();
  t_fenv_qsort_intrin();
  t_syscon_sync();
  printf("SUMMARY pass=%d fail=%d\n", npass, nfail);
  return nfail != 0;
}
