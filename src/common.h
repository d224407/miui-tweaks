
#ifndef MT_COMMON_H
#define MT_COMMON_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>

#define MT_LINE_MAX 1024
#ifndef MT_LOGFILE
#define MT_LOGFILE  "/storage/emulated/0/Android/miui_tweaks.log"
#endif


static void mt_log(int level, const char *fmt, ...) {
  FILE *f = fopen(MT_LOGFILE, "a");
  if (!f) return;
  time_t now = time(NULL);
  struct tm tmv;
  localtime_r(&now, &tmv);
  char ts[32];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
  const char *tag = level == 1 ? "INFO" : level == 2 ? "WARN" : level == 3 ? "ERROR" : "?";
  fprintf(f, "[%s] [%s] ", ts, tag);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
  va_end(ap);
  fprintf(f, "\n");
  fclose(f);
}


static int mt_conf_get(const char *path, const char *key, char *out, size_t outlen) {
  FILE *f = fopen(path, "r");
  if (!f) return 0;
  char line[MT_LINE_MAX];
  size_t klen = strlen(key);
  int found = 0;
  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
      p += klen + 1;
      size_t n = strcspn(p, "\r\n");
      if (n >= outlen) n = outlen - 1;
      memcpy(out, p, n);
      out[n] = '\0';
      found = 1; 
    }
  }
  fclose(f);
  return found;
}

static int mt_is_on(const char *path, const char *key) {
  char v[8] = {0};
  if (!mt_conf_get(path, key, v, sizeof(v))) return 0;
  return v[0] == '1' && v[1] == '\0';
}


static int mt_run(char *const argv[], int silent) {
  pid_t pid = fork();
  if (pid < 0) return -1;
  if (pid == 0) {
    if (silent) {
      int devnull = open("/dev/null", O_WRONLY);
      if (devnull >= 0) { dup2(devnull, 1); dup2(devnull, 2); close(devnull); }
    }
    execv(argv[0], argv);
    _exit(127); 
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) return -1;
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  return -1;
}


/* resetprop-rs (v0.6.x) lives next to the tool binaries in system/bin and is
 * always called by full path - a bare "resetprop" would resolve to Magisk's or
 * KernelSU's own build, which has none of the v6 flags used below.
 * MT_RESETPROP overrides the path (testing / manual use). */
static const char *mt_resetprop(char *buf, size_t buflen) {
  const char *env = getenv("MT_RESETPROP");
  if (env && *env) { snprintf(buf, buflen, "%s", env); return buf; }
  char self[512];
  ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
  if (n > 0) {
    self[n] = '\0';
    char *sl = strrchr(self, '/');
    if (sl) {
      *sl = '\0';
      snprintf(buf, buflen, "%s/resetprop-rs", self);
      if (access(buf, X_OK) == 0) return buf;
    }
  }
  snprintf(buf, buflen, "/data/adb/modules/miui_tweaks/system/bin/resetprop-rs");
  if (access(buf, X_OK) == 0) return buf;
  return NULL;
}

static int mt_track_has(const char *track_path, const char *tag, const char *name) {
  FILE *f = fopen(track_path, "r");
  if (!f) return 0;
  char want[MT_LINE_MAX], line[MT_LINE_MAX];
  snprintf(want, sizeof(want), "%s %s", tag, name);
  int found = 0;
  while (fgets(line, sizeof(line), f)) {
    line[strcspn(line, "\r\n")] = '\0';
    if (strcmp(line, want) == 0) { found = 1; break; }
  }
  fclose(f);
  return found;
}

/* Records "tag name" so revert can delete exactly what this tweak set.
 * Skips lines already present so the daily re-apply doesn't grow the file. */
static void mt_track_add(const char *track_path, const char *tag, const char *name) {
  if (mt_track_has(track_path, tag, name)) return;
  FILE *tf = fopen(track_path, "a");
  if (tf) { fprintf(tf, "%s %s\n", tag, name); fclose(tf); }
}

/* resetprop-rs -n NAME VALUE  (-n = write without serial bump / futex wake) */
static int mt_set_prop(const char *track_path, const char *tag, const char *name, const char *value) {
  char rp[512];
  if (!mt_resetprop(rp, sizeof(rp))) {
    mt_log(3, "set_prop: resetprop-rs not found, cannot set %s (tag %s)", name, tag);
    return -1;
  }
  char *argv[] = { rp, (char *)"-n", (char *)name, (char *)value, NULL };
  int rc = mt_run(argv, 1);
  if (rc == 0) mt_track_add(track_path, tag, name);
  else mt_log(3, "set_prop: resetprop-rs -n %s %s failed (rc=%d, tag %s)", name, value, rc, tag);
  return rc;
}

/* Sets a whole { {name, value}, ..., {NULL, NULL} } table with ONE resetprop-rs
 * run (-f FILE, one name=value per line) instead of one fork+exec per prop.
 * If the batch run fails it falls back to setting each prop on its own so one
 * bad entry cannot cost the rest, and every failure still gets logged.
 * Returns how many props failed. */
static int mt_set_props(const char *track_path, const char *tag, const char *const table[][2]) {
  int count = 0;
  while (table[count][0]) count++;
  if (!count) return 0;

  char rp[512];
  if (!mt_resetprop(rp, sizeof(rp))) {
    mt_log(3, "set_props: resetprop-rs not found, %d props of %s not applied", count, tag);
    return count;
  }

  char batch[600];
  snprintf(batch, sizeof(batch), "%s.batch", track_path);
  FILE *bf = fopen(batch, "w");
  if (bf) {
    for (int i = 0; i < count; i++) fprintf(bf, "%s=%s\n", table[i][0], table[i][1]);
    fclose(bf);
    char *argv[] = { rp, (char *)"-n", (char *)"-f", batch, NULL };
    int rc = mt_run(argv, 1);
    remove(batch);
    if (rc == 0) {
      for (int i = 0; i < count; i++) mt_track_add(track_path, tag, table[i][0]);
      return 0;
    }
    mt_log(2, "set_props: batch run for %s failed (rc=%d), retrying %d props one by one", tag, rc, count);
  } else {
    mt_log(2, "set_props: cannot write %s, setting %d props of %s one by one", batch, count, tag);
  }

  int failed = 0;
  for (int i = 0; i < count; i++) if (mt_set_prop(track_path, tag, table[i][0], table[i][1]) != 0) failed++;
  return failed;
}

/* resetprop-rs --delete-if-exist NAME  (exit 0 when the prop is already gone) */
static void mt_delete_prop(const char *name) {
  char rp[512];
  if (!mt_resetprop(rp, sizeof(rp))) { mt_log(3, "delete_prop: resetprop-rs not found, %s not deleted", name); return; }
  char *argv[] = { rp, (char *)"--delete-if-exist", (char *)name, NULL };
  int rc = mt_run(argv, 1);
  if (rc != 0) mt_log(3, "delete_prop: resetprop-rs --delete-if-exist %s failed (rc=%d)", name, rc);
}

/* Deletes every prop recorded under `tag` and rewrites the track file
 * without those lines. */
static void mt_revert_props_tag(const char *track_path, const char *tag) {
  FILE *f = fopen(track_path, "r");
  if (!f) return;
  char tmp_path[512];
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", track_path);
  FILE *tmp = fopen(tmp_path, "w");
  char line[MT_LINE_MAX];
  size_t taglen = strlen(tag);
  while (fgets(line, sizeof(line), f)) {
    if (strncmp(line, tag, taglen) == 0 && line[taglen] == ' ') {
      char *name = line + taglen + 1;
      size_t n = strcspn(name, "\r\n");
      name[n] = '\0';
      if (n) mt_delete_prop(name);
    } else if (tmp) {
      fputs(line, tmp);
    }
  }
  fclose(f);
  if (tmp) { fclose(tmp); rename(tmp_path, track_path); }
}

#endif 
