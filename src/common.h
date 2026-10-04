
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


static const char *mt_which(const char *name, char *buf, size_t buflen) {
  const char *dirs[] = {
    "/data/adb/ksu/bin", "/data/adb/ap/bin", "/data/adb/magisk",
    "/system/bin", "/system/xbin", NULL
  };
  for (int i = 0; dirs[i]; i++) {
    snprintf(buf, buflen, "%s/%s", dirs[i], name);
    if (access(buf, X_OK) == 0) return buf;
  }
  snprintf(buf, buflen, "%s", name); 
  return buf;
}


static int mt_set_prop(const char *track_path, const char *tag, const char *name, const char *value) {
  char bin[256];
  const char *p = mt_which("resetprop", bin, sizeof(bin));
  char *argv[6];
  int n = 0;
  argv[n++] = (char *)p;
  if (access(p, X_OK) == 0 && strstr(p, "resetprop")) argv[n++] = (char *)"-n";
  argv[n++] = (char *)name;
  argv[n++] = (char *)value;
  argv[n] = NULL;
  int rc = mt_run(argv, 1);
  if (rc != 0) {
    
    char *argv2[] = { (char *)"/system/bin/setprop", (char *)name, (char *)value, NULL };
    rc = mt_run(argv2, 1);
  }
  if (rc == 0) {
    FILE *tf = fopen(track_path, "a");
    if (tf) { fprintf(tf, "%s %s\n", tag, name); fclose(tf); }
  } else {
    mt_log(3, "set_prop: failed to set %s=%s (tag %s)", name, value, tag);
  }
  return rc;
}

static void mt_delete_prop(const char *name) {
  char bin[256];
  const char *p = mt_which("resetprop", bin, sizeof(bin));
  char *argv[] = { (char *)p, (char *)"--delete", (char *)name, NULL };
  if (mt_run(argv, 1) != 0) {
    char *argv2[] = { (char *)p, (char *)"-d", (char *)name, NULL };
    mt_run(argv2, 1);
  }
}


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
