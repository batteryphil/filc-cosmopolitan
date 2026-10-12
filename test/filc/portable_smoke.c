// Portability smoke test for filc-cosmopolitan APE binaries.
// Each check prints exactly one line "PASS name" / "FAIL name: detail" and
// never aborts the run, so a single broken feature can't hide the others.
// The final line is "SUMMARY pass=N fail=M".
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

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
  close(fd);
  if (n != (ssize_t)sizeof msg || memcmp(buf, msg, sizeof msg) != 0)
    return fail("file_read", "content mismatch");
  pass("file_read");

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
  clock_gettime(CLOCK_MONOTONIC, &a);
  if (nanosleep(&d, NULL) != 0)
    return fail("nanosleep", "nanosleep");
  clock_gettime(CLOCK_MONOTONIC, &b);
  long ms = (b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000;
  if (ms < 15)
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

static void t_thread(void) {
  pthread_t th;
  int x = 41;
  void *ret = NULL;
  if (pthread_create(&th, NULL, thread_main, &x) != 0)
    return fail("pthread", "pthread_create");
  if (pthread_join(th, &ret) != 0 || ret != &x || x != 42)
    return fail("pthread", "join/result");
  pass("pthread");
}

static volatile sig_atomic_t got_sig;
static void on_usr1(int sig) {
  got_sig = sig;
}

static void t_signal(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = on_usr1;
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
  pass("socket_loopback");
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
  printf("SUMMARY pass=%d fail=%d\n", npass, nfail);
  return nfail != 0;
}
