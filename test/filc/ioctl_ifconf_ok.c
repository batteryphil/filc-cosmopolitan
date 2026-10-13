// Legitimate SIOCGIFCONF use must keep working after the Fil-C bounds check:
// (1) NULL-buffer size query, (2) a correctly sized buffer.
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    return 2;

  struct ifconf ifc = {0};
  int need = 0;
  ifc.ifc_buf = NULL;  // size query (Linux)
  if (ioctl(fd, SIOCGIFCONF, &ifc) == 0 && ifc.ifc_len > 0) {
    need = ifc.ifc_len;
    printf("size query: kernel needs %d bytes\n", need);
  } else {
    // BSDs (FreeBSD/NetBSD/OpenBSD) reject NULL ifc_buf with EINVAL; use standard size.
    need = 1024;
    printf("size query unsupported on this OS (BSD), using %d bytes\n", need);
  }

  ifc.ifc_len = need;
  ifc.ifc_buf = malloc(need);
  if (ioctl(fd, SIOCGIFCONF, &ifc) != 0) {
    perror("fill");
    return 1;
  }
  int n = ifc.ifc_len / (int)sizeof(struct ifreq);
  for (int i = 0; i < n; i++)
    printf("  interface: %s\n", ifc.ifc_req[i].ifr_name);
  printf("RESULT: OK (%d interfaces)\n", n);
  close(fd);
  return 0;
}
