// Probe: does an ioctl whose argument struct *contains* a pointer let the
// kernel write past that inner pointer's Fil-C capability bounds?
//
// SIOCGIFCONF takes struct ifconf { int ifc_len; char *ifc_buf; }.  The Fil-C
// runtime guards the outer 16-byte struct, but the kernel follows ifc_buf
// itself and writes up to ifc_len bytes there.
//
// Expected if SAFE:   a filc safety panic before/at the ioctl.
// Expected if UNSAFE: ioctl succeeds and reports writing > 8 bytes into an
//                     8-byte object (kernel bypassed the capability).
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void) {
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    perror("socket");
    return 2;
  }

  char *small = malloc(8);          // capability covers exactly 8 bytes
  char *victim = malloc(64);        // neighbouring allocation
  memset(small, 0xAA, 8);
  memset(victim, 0x55, 64);

  struct ifconf ifc;
  ifc.ifc_len = 4096;               // lie: claim the buffer is 4096 bytes
  ifc.ifc_buf = small;

  int rc = ioctl(fd, SIOCGIFCONF, &ifc);
  printf("ioctl rc=%d, kernel reports writing %d bytes into an 8-byte object\n",
         rc, ifc.ifc_len);

  int victim_changed = 0;
  for (int i = 0; i < 64; i++)
    if ((unsigned char)victim[i] != 0x55)
      victim_changed = 1;
  printf("neighbouring allocation modified: %s\n", victim_changed ? "YES" : "no");

  if (rc == 0 && ifc.ifc_len > 8)
    printf("RESULT: UNSAFE - kernel wrote past the capability with no Fil-C trap\n");
  else
    printf("RESULT: no overflow observed\n");
  close(fd);
  return 0;
}
