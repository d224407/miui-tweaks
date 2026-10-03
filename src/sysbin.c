
#include "common.h"
#include <dirent.h>
#include <limits.h>

typedef struct { const char *flag; const char *bins[16]; } group_t;

static const group_t GROUPS[] = {
  {"STUB_LOG", {"log","logcat","logcatd","logger","logname","logwrapper","logpersist.cat","logpersist.start","logpersist.stop", NULL}},
  {"STUB_TRACED", {"traced","traced_probes","traced_perf","atrace", NULL}},
  {"STUB_DEBUG", {"debuggerd","tombstoned","crash_dump32","crash_dump64", NULL}},
  {"STUB_BUGREPORT", {"bugreport","bugreportz","bugreport_procdump","dumpstate","dmesgd","dmesg","lpdump","lpdumpd", NULL}},
  {"STUB_NETDIAG", {"tcpdump","tracepath","tracepath6","traceroute6","diag_socket_log","i2cdump","dmabuf_dump","notify_traceur.sh", NULL}},
  {NULL, {NULL}}
};

static int mkdir_p(const char *path) {
  char tmp[PATH_MAX];
  snprintf(tmp, sizeof(tmp), "%s", path);
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
  }
  return mkdir(tmp, 0755) == 0 || errno == EEXIST ? 0 : -1;
}

static int write_stub(const char *bindir, const char *name) {
  char dst[PATH_MAX];
  snprintf(dst, sizeof(dst), "%s/%s", bindir, name);
  if (mkdir_p(bindir) != 0) { mt_log(3, "sysbin: failed to create %s", bindir); return -1; }
  FILE *f = fopen(dst, "w");
  if (!f) { mt_log(3, "sysbin: failed writing stub %s", name); return -1; }
  fputs("#!/system/bin/sh\nexit 0\n", f);
  fclose(f);
  chmod(dst, 0755);
  return 0;
}

static int dir_has_files(const char *path) {
  DIR *d = opendir(path);
  if (!d) return 0;
  struct dirent *e;
  int any = 0;
  while ((e = readdir(d))) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
    any = 1; break;
  }
  closedir(d);
  return any;
}

static void apply_all(const char *conf, const char *moddir) {
  char bindir[PATH_MAX];
  snprintf(bindir, sizeof(bindir), "%s/system/bin", moddir);
  int master = mt_is_on(conf, "SYSBIN_MASTER");
  for (int g = 0; GROUPS[g].flag; g++) {
    int flag_on = mt_is_on(conf, GROUPS[g].flag);
    for (int i = 0; GROUPS[g].bins[i]; i++) {
      char dst[PATH_MAX];
      snprintf(dst, sizeof(dst), "%s/%s", bindir, GROUPS[g].bins[i]);
      if (master && flag_on) write_stub(bindir, GROUPS[g].bins[i]);
      else remove(dst);
    }
  }
  if (dir_has_files(bindir)) mt_log(1, "sysbin_stubs: applied (reboot to take effect)");
  rmdir(bindir); 
}

int main(int argc, char **argv) {
  if (argc < 5) { fprintf(stderr, "usage: sysbin <conf> <track> <moddir> early|late|set KEY VALUE\n"); return 2; }
  const char *conf = argv[1], *moddir = argv[3], *mode = argv[4];

  if (strcmp(mode, "early") == 0 || strcmp(mode, "late") == 0 || strcmp(mode, "set") == 0) {
    apply_all(conf, moddir);
    return 0;
  }

  fprintf(stderr, "sysbin: unknown mode %s\n", mode);
  return 2;
}
