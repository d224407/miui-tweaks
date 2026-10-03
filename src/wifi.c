/* wifi.c - Qualcomm WCNSS_qcom_cfg.ini overlay patcher.
 * Usage: wifi <conf> <track> <moddir> early|late|set <KEY> <VALUE>
 * <track> is unused (nothing here is a resetprop) but kept for a
 * consistent argv shape across every tool.
 */
#define _GNU_SOURCE
#include "common.h"
#include <ftw.h>
#include <dirent.h>
#include <limits.h>

static char g_moddir[PATH_MAX];
static char g_cache[PATH_MAX];
static char g_found[32][PATH_MAX];
static int g_found_n = 0;

static int ftw_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
  (void)ftwbuf;
  if (typeflag == FTW_F && S_ISREG(sb->st_mode)) {
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    if (strcmp(base, "WCNSS_qcom_cfg.ini") == 0 && g_found_n < 32) {
      snprintf(g_found[g_found_n++], PATH_MAX, "%s", path);
    }
  }
  return 0;
}

/* Mirrors wifi_find_cfg(): cached in g_cache if present, else an
 * FTW_PHYS (no symlink following, matching the shell's `-L` skip) walk
 * of /system and /vendor. */
static void find_cfg(void) {
  FILE *cf = fopen(g_cache, "r");
  if (cf) {
    char line[PATH_MAX];
    while (fgets(line, sizeof(line), cf)) {
      size_t n = strcspn(line, "\r\n");
      if (n && g_found_n < 32) { memcpy(g_found[g_found_n], line, n); g_found[g_found_n][n] = '\0'; g_found_n++; }
    }
    fclose(cf);
    if (g_found_n > 0) return;
  }
  nftw("/system", ftw_cb, 16, FTW_PHYS);
  nftw("/vendor", ftw_cb, 16, FTW_PHYS);
  FILE *wf = fopen(g_cache, "w");
  if (wf) {
    for (int i = 0; i < g_found_n; i++) fprintf(wf, "%s\n", g_found[i]);
    fclose(wf);
  }
}

/* Mirrors wifi_overlay_path(): /vendor/x -> $MODDIR/system/vendor/x, else $MODDIR/x */
static void overlay_path(const char *src, char *out, size_t outlen) {
  if (strncmp(src, "/vendor/", 8) == 0) snprintf(out, outlen, "%s/system%s", g_moddir, src);
  else snprintf(out, outlen, "%s%s", g_moddir, src);
}

static int mkdir_p(const char *path) {
  char tmp[PATH_MAX];
  snprintf(tmp, sizeof(tmp), "%s", path);
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
  }
  return mkdir(tmp, 0755) == 0 || errno == EEXIST ? 0 : -1;
}

static int copy_file(const char *src, const char *dst) {
  FILE *in = fopen(src, "rb");
  if (!in) return -1;
  FILE *out = fopen(dst, "wb");
  if (!out) { fclose(in); return -1; }
  char buf[8192];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, n, out);
  fclose(in);
  fclose(out);
  return 0;
}

/* Same algorithm as the shell wifi_set_key(): replace if the key exists;
 * else insert right before a bare "END" line (the parser stops reading
 * there); else just append. */
static void set_key(const char *path, const char *key, const char *value) {
  FILE *in = fopen(path, "r");
  if (!in) { mt_log(3, "wifi_qcom_fix: cannot open %s to set %s", path, key); return; }
  char tmp_path[PATH_MAX];
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
  FILE *out = fopen(tmp_path, "w");
  if (!out) { fclose(in); mt_log(3, "wifi_qcom_fix: cannot write temp file for %s", path); return; }

  char line[MT_LINE_MAX];
  size_t klen = strlen(key);
  int replaced = 0, inserted_before_end = 0;
  long end_line_pos_in_out = -1;

  /* First pass: replace in place if present. */
  while (fgets(line, sizeof(line), in)) {
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (!replaced && strncmp(p, key, klen) == 0 && p[klen] == '=') {
      fprintf(out, "%s=%s\n", key, value);
      replaced = 1;
    } else {
      /* Detect a bare END line to insert before, if we still haven't
         replaced anything by the time we reach it. */
      char trimmed[MT_LINE_MAX];
      size_t n = strcspn(line, "\r\n");
      memcpy(trimmed, line, n); trimmed[n] = '\0';
      char *t = trimmed;
      while (*t == ' ' || *t == '\t') t++;
      if (!replaced && !inserted_before_end && strcmp(t, "END") == 0) {
        fprintf(out, "%s=%s\n", key, value);
        inserted_before_end = 1;
        end_line_pos_in_out = 1;
      }
      fputs(line, out);
    }
  }
  (void)end_line_pos_in_out;
  if (!replaced && !inserted_before_end) {
    fprintf(out, "%s=%s\n", key, value);
  }
  fclose(in);
  fclose(out);
  rename(tmp_path, path);
}

static void patch_one(const char *conf, const char *cfg, int enable) {
  (void)conf; /* no longer needed - every key below is unconditional now */
  char dst[PATH_MAX];
  overlay_path(cfg, dst, sizeof(dst));

  if (!enable) { remove(dst); return; }

  char src[PATH_MAX];
  snprintf(src, sizeof(src), "%s", cfg);
  char mirror[PATH_MAX];
  snprintf(mirror, sizeof(mirror), "/sbin/.magisk/mirror%s", cfg);
  struct stat st;
  if (stat(mirror, &st) == 0) snprintf(src, sizeof(src), "%s", mirror);

  char dstdir[PATH_MAX];
  snprintf(dstdir, sizeof(dstdir), "%s", dst);
  char *slash = strrchr(dstdir, '/');
  if (slash) *slash = '\0';
  if (mkdir_p(dstdir) != 0) { mt_log(3, "wifi_qcom_fix: mkdir failed for %s", dst); return; }

  if (copy_file(src, dst) != 0) { mt_log(3, "wifi_qcom_fix: cp failed, %s -> %s", src, dst); return; }

  set_key(dst, "hostArpOffload", "0");
  set_key(dst, "hostNsOffload", "0");
  set_key(dst, "gMCAddrListEnable", "1");
  set_key(dst, "gEnablePowerSaveOffload", "5");
  set_key(dst, "gRuntimePM", "1");
  set_key(dst, "RoamRssiDiff", "3");
  set_key(dst, "g11dSupportEnabled", "0");
  set_key(dst, "RTSThreshold", "1048576");
  set_key(dst, "gActiveMaxChannelTime", "40");
  set_key(dst, "gActiveMinChannelTime", "20");
  set_key(dst, "gMaxConcurrentActiveSessions", "2");
  set_key(dst, "rx_wakelock_timeout", "0");

  chmod(dst, 0644);
  if (stat(src, &st) == 0) chown(dst, st.st_uid, st.st_gid);
  /* SELinux context copy has no simple libc call - shell out to chcon,
     same as the original script did. */
  char *a1[] = { (char *)"/system/bin/chcon", (char *)"--reference", (char *)src, (char *)dst, NULL };
  if (mt_run(a1, 1) != 0) {
    char *a2[] = { (char *)"/system/bin/chcon", (char *)"u:object_r:vendor_configs_file:s0", (char *)dst, NULL };
    mt_run(a2, 1);
  }
  mt_log(1, "wifi_qcom_fix: patched overlay for %s (reboot to take effect)", cfg);
}

static void run_wifi_fix(const char *conf) {
  int enable = mt_is_on(conf, "WIFI_QCOM_FIX");
  struct stat st;
  if (!enable && stat(g_cache, &st) != 0) return; /* never applied, nothing to clean up */
  find_cfg();
  int found = 0;
  for (int i = 0; i < g_found_n; i++) {
    if (enable) found = 1;
    patch_one(conf, g_found[i], enable);
  }
  if (enable && !found) mt_log(2, "wifi_qcom_fix: WCNSS_qcom_cfg.ini not found (non-Qualcomm device?)");
}

int main(int argc, char **argv) {
  if (argc < 5) { fprintf(stderr, "usage: wifi <conf> <track> <moddir> early|late|set KEY VALUE\n"); return 2; }
  const char *conf = argv[1], *moddir = argv[3], *mode = argv[4];

  snprintf(g_moddir, sizeof(g_moddir), "%s", moddir);
  snprintf(g_cache, sizeof(g_cache), "%s/config/.wifi_cfg_paths", g_moddir);

  if (strcmp(mode, "early") == 0 || strcmp(mode, "late") == 0) {
    run_wifi_fix(conf);
    return 0;
  }

  if (strcmp(mode, "set") == 0) {
    /* Only WIFI_QCOM_FIX exists now - on applies every fix at once, off
       removes the overlay file. */
    run_wifi_fix(conf);
    return 0;
  }

  fprintf(stderr, "wifi: unknown mode %s\n", mode);
  return 2;
}
