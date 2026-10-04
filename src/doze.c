/* doze.c - put Google Play services under Doze.
 * Usage: doze <conf> <track> <moddir> early|late|set <KEY> <VALUE>
 */
#define _GNU_SOURCE
#include "common.h"
#include <dirent.h>
#include <limits.h>
#include <strings.h>

#ifndef DOZE_ROOT
#define DOZE_ROOT ""
#endif

#define GMS_PKG "com.google.android.gms"
#define TARGET  "allow-in-power-save package=\"" GMS_PKG "\""
#define MAX_FILES 64

static char g_moddir[PATH_MAX];
static char g_cache[PATH_MAX];
static char g_marker[PATH_MAX];
static char g_old[MAX_FILES][PATH_MAX];
static char g_new[MAX_FILES][PATH_MAX];

static const char *PARTS[] = { "/system", "/system_ext", "/product", "/vendor", NULL };
static const char *SUBDIRS[] = { "/etc/sysconfig", "/etc/permissions", NULL };

static char *read_all(const char *path, size_t *outlen) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
  long sz = ftell(f);
  if (sz < 0 || sz > 8 * 1024 * 1024) { fclose(f); return NULL; }
  rewind(f);
  char *buf = malloc((size_t)sz + 1);
  if (!buf) { fclose(f); return NULL; }
  size_t n = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[n] = '\0';
  if (outlen) *outlen = n;
  return buf;
}

static int mkdir_p(const char *path) {
  char tmp[PATH_MAX];
  snprintf(tmp, sizeof(tmp), "%s", path);
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
  }
  return (mkdir(tmp, 0755) == 0 || errno == EEXIST) ? 0 : -1;
}

/* Copies src to dst without any line containing TARGET. */
static int write_patched(const char *src, const char *dst, int *removed) {
  char *buf = read_all(src, NULL);
  if (!buf) return -1;
  FILE *out = fopen(dst, "w");
  if (!out) { free(buf); return -1; }
  *removed = 0;
  char *p = buf;
  while (*p) {
    char *nl = strchr(p, '\n');
    size_t n = nl ? (size_t)(nl - p) + 1 : strlen(p);
    if (nl) *nl = '\0';
    int drop = strstr(p, TARGET) != NULL;
    if (nl) *nl = '\n';
    if (drop) (*removed)++; else fwrite(p, 1, n, out);
    p += n;
  }
  fclose(out);
  free(buf);
  return 0;
}

/* /vendor/etc/x -> $MODDIR/system/vendor/etc/x ; /system/etc/x -> $MODDIR/system/etc/x */
static void overlay_path(const char *orig, char *out, size_t n) {
  if (strncmp(orig, "/system/", 8) == 0) snprintf(out, n, "%s%s", g_moddir, orig);
  else snprintf(out, n, "%s/system%s", g_moddir, orig);
}

/* Prefer Magisk's untouched mirror so a file we already overlaid is still read in its original form. */
static void src_path(const char *orig, char *out, size_t n) {
  static const char *mirrors[] = { "/debug_ramdisk/.magisk/mirror", "/sbin/.magisk/mirror", "/.magisk/mirror", NULL };
  struct stat st;
  if (!*DOZE_ROOT) {
    for (int i = 0; mirrors[i]; i++) {
      snprintf(out, n, "%s%s", mirrors[i], orig);
      if (stat(out, &st) == 0) return;
    }
  }
  snprintf(out, n, "%s%s", DOZE_ROOT, orig);
}

static int has_xml_ext(const char *name) {
  size_t l = strlen(name);
  return l > 4 && strcasecmp(name + l - 4, ".xml") == 0;
}

static int scan(char found[][PATH_MAX]) {
  int n = 0;
  for (int p = 0; PARTS[p]; p++) {
    for (int s = 0; SUBDIRS[s]; s++) {
      char dir[PATH_MAX];
      snprintf(dir, sizeof(dir), "%s%s%s", DOZE_ROOT, PARTS[p], SUBDIRS[s]);
      DIR *d = opendir(dir);
      if (!d) continue;
      struct dirent *e;
      while ((e = readdir(d)) && n < MAX_FILES) {
        if (!has_xml_ext(e->d_name)) continue;
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        char *buf = read_all(full, NULL);
        if (!buf) continue;
        int hit = strstr(buf, TARGET) != NULL;
        free(buf);
        if (hit) snprintf(found[n++], PATH_MAX, "%s%s/%s", PARTS[p], SUBDIRS[s], e->d_name);
      }
      closedir(d);
    }
  }
  return n;
}

static int read_cache(char out[][PATH_MAX]) {
  FILE *f = fopen(g_cache, "r");
  if (!f) return 0;
  int n = 0;
  char line[PATH_MAX];
  while (n < MAX_FILES && fgets(line, sizeof(line), f)) {
    size_t l = strcspn(line, "\r\n");
    if (!l) continue;
    memcpy(out[n], line, l);
    out[n][l] = '\0';
    n++;
  }
  fclose(f);
  return n;
}

static void write_cache(char list[][PATH_MAX], int n) {
  FILE *f = fopen(g_cache, "w");
  if (!f) { mt_log(3, "gms_doze: cannot write %s", g_cache); return; }
  for (int i = 0; i < n; i++) fprintf(f, "%s\n", list[i]);
  fclose(f);
}

static void prune_empty_parents(const char *path) {
  char tmp[PATH_MAX], stop[PATH_MAX];
  snprintf(tmp, sizeof(tmp), "%s", path);
  snprintf(stop, sizeof(stop), "%s/system", g_moddir);
  for (;;) {
    char *sl = strrchr(tmp, '/');
    if (!sl) return;
    *sl = '\0';
    if (strlen(tmp) <= strlen(stop)) return;
    if (rmdir(tmp) != 0) return;
  }
}

static void remove_overlay(const char *orig) {
  char dst[PATH_MAX];
  overlay_path(orig, dst, sizeof(dst));
  if (remove(dst) == 0) mt_log(1, "gms_doze: removed overlay for %s", orig);
  prune_empty_parents(dst);
}

static int make_overlay(const char *orig) {
  char src[PATH_MAX], dst[PATH_MAX], dir[PATH_MAX];
  src_path(orig, src, sizeof(src));
  overlay_path(orig, dst, sizeof(dst));
  snprintf(dir, sizeof(dir), "%s", dst);
  char *sl = strrchr(dir, '/');
  if (sl) *sl = '\0';
  if (mkdir_p(dir) != 0) { mt_log(3, "gms_doze: cannot create %s", dir); return -1; }

  int removed = 0;
  if (write_patched(src, dst, &removed) != 0) {
    mt_log(3, "gms_doze: failed to patch %s (read from %s)", orig, src);
    return -1;
  }
  chmod(dst, 0644);
  struct stat st;
  if (stat(src, &st) == 0) chown(dst, st.st_uid, st.st_gid);
  char *a1[] = { (char *)"/system/bin/chcon", (char *)"--reference", src, dst, NULL };
  if (mt_run(a1, 1) != 0) {
    char *a2[] = { (char *)"/system/bin/chcon", (char *)"u:object_r:system_file:s0", dst, NULL };
    mt_run(a2, 1);
  }
  if (removed) mt_log(1, "gms_doze: patched %s (%d line removed)", orig, removed);
  else mt_log(1, "gms_doze: overlay for %s refreshed from its already-patched copy", orig);
  return 0;
}

static int in_list(char list[][PATH_MAX], int n, const char *s) {
  for (int i = 0; i < n; i++) if (strcmp(list[i], s) == 0) return 1;
  return 0;
}

static int booted(void) {
  FILE *p = popen("/system/bin/getprop sys.boot_completed 2>/dev/null", "r");
  if (!p) return 0;
  int c = fgetc(p);
  pclose(p);
  return c == '1';
}

static void overlays_from_cache(void) {
  int n = read_cache(g_new);
  if (n == 0) { n = scan(g_new); write_cache(g_new, n); }
  if (n == 0) mt_log(2, "gms_doze: no sysconfig file whitelists GMS for power-save on this ROM, only the deviceidle whitelist changes");
  for (int i = 0; i < n; i++) make_overlay(g_new[i]);
}

static void overlays_remove_all(void) {
  int n = read_cache(g_old);
  for (int i = 0; i < n; i++) remove_overlay(g_old[i]);
}

/* Boot time: originals are visible (module not mounted yet), so rescan fresh. */
static void overlays_boot_refresh(void) {
  int no = read_cache(g_old);
  int nn = scan(g_new);
  for (int i = 0; i < no; i++) if (!in_list(g_new, nn, g_old[i])) remove_overlay(g_old[i]);
  write_cache(g_new, nn);
  if (nn == 0) mt_log(2, "gms_doze: no sysconfig file whitelists GMS for power-save on this ROM, only the deviceidle whitelist changes");
  for (int i = 0; i < nn; i++) make_overlay(g_new[i]);
}

static int whitelist(const char *sign) {
  char arg[128];
  snprintf(arg, sizeof(arg), "%s%s", sign, GMS_PKG);
  char *argv[] = { (char *)"/system/bin/dumpsys", (char *)"deviceidle", (char *)"whitelist", arg, NULL };
  return mt_run(argv, 1);
}

static void whitelist_remove(void) {
  if (whitelist("-") != 0) { mt_log(3, "gms_doze: dumpsys deviceidle whitelist -%s failed", GMS_PKG); return; }
  FILE *m = fopen(g_marker, "w");
  if (m) { fputs("1\n", m); fclose(m); }
  mt_log(1, "gms_doze: %s removed from deviceidle whitelist", GMS_PKG);
}

static void whitelist_restore(void) {
  if (access(g_marker, F_OK) != 0) return;
  if (whitelist("+") != 0) { mt_log(3, "gms_doze: dumpsys deviceidle whitelist +%s failed", GMS_PKG); return; }
  remove(g_marker);
  mt_log(1, "gms_doze: %s restored to deviceidle whitelist", GMS_PKG);
}

int main(int argc, char **argv) {
  if (argc < 5) { fprintf(stderr, "usage: doze <conf> <track> <moddir> early|late|set KEY VALUE\n"); return 2; }
  const char *conf = argv[1], *moddir = argv[3], *mode = argv[4];
  snprintf(g_moddir, sizeof(g_moddir), "%s", moddir);
  snprintf(g_cache, sizeof(g_cache), "%s/config/.doze_paths", g_moddir);
  snprintf(g_marker, sizeof(g_marker), "%s/config/.doze_wl", g_moddir);

  if (strcmp(mode, "early") == 0) {
    if (!mt_is_on(conf, "GMS_DOZE")) { overlays_remove_all(); return 0; }
    if (booted()) overlays_from_cache(); /* "Apply now" after boot: originals are hidden by our own mount */
    else overlays_boot_refresh();
    return 0;
  }

  if (strcmp(mode, "late") == 0) {
    if (mt_is_on(conf, "GMS_DOZE")) whitelist_remove();
    else whitelist_restore();
    return 0;
  }

  if (strcmp(mode, "set") == 0) {
    if (argc < 7) { fprintf(stderr, "usage: doze <conf> <track> <moddir> set GMS_DOZE VALUE\n"); return 2; }
    if (strcmp(argv[5], "GMS_DOZE") != 0) { mt_log(2, "doze: unknown key %s", argv[5]); return 1; }
    if (strcmp(argv[6], "1") == 0) { overlays_from_cache(); whitelist_remove(); }
    else { overlays_remove_all(); whitelist_restore(); }
    return 0;
  }

  fprintf(stderr, "doze: unknown mode %s\n", mode);
  return 2;
}
