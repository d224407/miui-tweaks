/* sleepboot.c - sleep for N seconds of real time, suspend included.
 * Usage: sleepboot <seconds>
 * A plain `sleep` runs on CLOCK_MONOTONIC, which stops while the phone is
 * suspended, so a "24h" shell sleep can really take days. This one waits on a
 * CLOCK_BOOTTIME timerfd instead: no polling, no wake-lock, no alarm - it just
 * returns the first time the phone is awake once the time is up.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/timerfd.h>

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: sleepboot <seconds>\n"); return 2; }
  char *end = NULL;
  long secs = strtol(argv[1], &end, 10);
  if (!end || *end || secs <= 0) { fprintf(stderr, "sleepboot: bad seconds '%s'\n", argv[1]); return 2; }

  int fd = timerfd_create(CLOCK_BOOTTIME, 0);
  if (fd < 0) { perror("sleepboot: timerfd_create"); return 1; }
  struct itimerspec it = { .it_interval = {0, 0}, .it_value = {secs, 0} };
  if (timerfd_settime(fd, 0, &it, NULL) != 0) { perror("sleepboot: timerfd_settime"); return 1; }

  uint64_t expirations = 0;
  for (;;) {
    ssize_t n = read(fd, &expirations, sizeof(expirations));
    if (n == (ssize_t)sizeof(expirations)) break;
    if (n < 0 && errno == EINTR) continue;
    perror("sleepboot: read");
    return 1;
  }
  return 0;
}
