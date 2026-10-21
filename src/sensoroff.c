
#include "common.h"

#define SERVICE_BIN "/system/bin/service"
#define GETPROP_BIN "/system/bin/getprop"

#define ERR(...) do { mt_log(3, __VA_ARGS__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while (0)

/* Run argv, collect stdout (stderr dropped) into out. Returns the exit status, -1 on failure. */
static int capture(char *const argv[], char *out, size_t outlen) {
  int fd[2];
  if (pipe(fd) < 0) return -1;
  pid_t pid = fork();
  if (pid < 0) { close(fd[0]); close(fd[1]); return -1; }
  if (pid == 0) {
    int dn = open("/dev/null", O_WRONLY);
    dup2(fd[1], 1);
    if (dn >= 0) { dup2(dn, 2); close(dn); }
    close(fd[0]); close(fd[1]);
    execv(argv[0], argv);
    _exit(127);
  }
  close(fd[1]);
  size_t n = 0;
  ssize_t r;
  while (n < outlen - 1 && (r = read(fd[0], out + n, outlen - 1 - n)) > 0) n += (size_t)r;
  out[n] = '\0';
  char junk[128];
  while (read(fd[0], junk, sizeof(junk)) > 0) {}
  close(fd[0]);
  int st = 0;
  if (waitpid(pid, &st, 0) < 0) return -1;
  return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

static void squash(char *s) {
  for (char *p = s; *p; p++) if (*p == '\n' || *p == '\r' || *p == '\t') *p = ' ';
  size_t n = strlen(s);
  while (n && s[n - 1] == ' ') s[--n] = '\0';
}

/* ISensorPrivacyManager.setSensorPrivacy(boolean) transaction number per
 * Android release. Same table as the original TurnOffSensors module; it
 * shifts whenever AOSP adds a method before it, so unknown releases are
 * refused instead of guessed. */
static int tx_code(const char *rel) {
  static const struct { const char *rel; int code; } map[] = {
    {"10", 4}, {"11", 4}, {"12", 8},
    {"13", 9}, {"14", 9}, {"15", 9}, {"16", 9}, {"17", 9},
    {NULL, 0}
  };
  for (int i = 0; map[i].rel; i++) if (strcmp(map[i].rel, rel) == 0) return map[i].code;
  return 0;
}

static int service_up(void) {
  char out[128];
  char *argv[] = { (char *)SERVICE_BIN, (char *)"check", (char *)"sensor_privacy", NULL };
  if (capture(argv, out, sizeof(out)) != 0) return 0;
  return strstr(out, "found") && !strstr(out, "not found");
}

/* "Parcel(00000000 ...)" or the older multi-line "Parcel(\n 0x00000000: 00000000 ..."
 * - the first word is the exception code, 0 = call went through. */
static int parcel_ok(const char *out) {
  const char *p = strstr(out, "Parcel(");
  if (!p) return 0;
  p += 7;
  while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
  if (p[0] == '0' && p[1] == 'x') {
    const char *c = strchr(p, ':');
    if (!c) return 0;
    p = c + 1;
    while (*p == ' ') p++;
  }
  return strncmp(p, "00000000", 8) == 0;
}

static int apply(int on, int wait_secs) {
  char rel[32];
  char *gp[] = { (char *)GETPROP_BIN, (char *)"ro.build.version.release", NULL };
  if (capture(gp, rel, sizeof(rel)) != 0) { ERR("sensors_off: getprop ro.build.version.release failed"); return 1; }
  squash(rel);
  int code = tx_code(rel);
  if (!code) { ERR("sensors_off: Android release '%s' not supported (10-17), nothing done", rel); return 1; }

  int up = service_up();
  for (int i = 0; !up && i < wait_secs; i++) { sleep(1); up = service_up(); }
  if (!up) { ERR("sensors_off: sensor_privacy service not found (waited %ds)", wait_secs); return 1; }

  char codestr[8];
  snprintf(codestr, sizeof(codestr), "%d", code);
  char *call[] = { (char *)SERVICE_BIN, (char *)"call", (char *)"sensor_privacy", codestr,
                   (char *)"i32", (char *)(on ? "1" : "0"), NULL };
  char out[256];
  int rc = capture(call, out, sizeof(out));
  if (rc != 0 || !parcel_ok(out)) {
    squash(out);
    ERR("sensors_off: service call sensor_privacy %d failed (rc=%d): %.120s", code, rc, out);
    return 1;
  }
  mt_log(1, "sensors_off: %s (Android %s, transaction %d)", on ? "enabled" : "disabled", rel, code);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: sensoroff <conf> <track> early|late|set KEY VALUE\n"); return 2; }
  const char *conf = argv[1], *mode = argv[3];

  if (strcmp(mode, "early") == 0) return 0;

  if (strcmp(mode, "late") == 0) {
    if (!mt_is_on(conf, "SENSORS_OFF")) return 0;
    return apply(1, 30);
  }

  if (strcmp(mode, "set") == 0) {
    if (argc < 6) { fprintf(stderr, "usage: sensoroff <conf> <track> set KEY VALUE\n"); return 2; }
    const char *key = argv[4], *val = argv[5];
    if (strcmp(key, "SENSORS_OFF") != 0) { mt_log(2, "sensoroff: unknown key %s", key); return 1; }
    return apply(strcmp(val, "1") == 0, 3);
  }

  fprintf(stderr, "sensoroff: unknown mode %s\n", mode);
  return 2;
}
