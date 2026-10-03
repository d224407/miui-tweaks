
#include "common.h"

static const char *CATEGORY_CONF_KEY(const char *cat) {
  static const struct { const char *cat, *key; } map[] = {
    {"ads","DISABLE_ADS"},{"tracking","DISABLE_TRACKING"},{"analytics","DISABLE_ANALYTICS"},
    {"reporting","DISABLE_REPORTING"},{"background","DISABLE_BACKGROUND"},{"update","DISABLE_UPDATE"},
    {"location","DISABLE_LOCATION"},{"geofence","DISABLE_GEOFENCE"},{"nearby","DISABLE_NEARBY"},
    {"cast","DISABLE_CAST"},{"discovery","DISABLE_DISCOVERY"},{"sync","DISABLE_SYNC"},
    {"cloud","DISABLE_CLOUD"},{"auth","DISABLE_AUTH"},{"wallet","DISABLE_WALLET"},
    {"payment","DISABLE_PAYMENT"},{"wear","DISABLE_WEAR"},{"fitness","DISABLE_FITNESS"},
    {NULL,NULL}
  };
  for (int i = 0; map[i].cat; i++) if (strcmp(map[i].cat, cat) == 0) return map[i].key;
  return NULL; 
}

static int gms_should_disable(const char *conf, const char *cat) {
  const char *key = CATEGORY_CONF_KEY(cat);
  if (!key) return 0;
  return mt_is_on(conf, key);
}

static void pm_toggle(const char *svc, int disable) {
  char *argv[] = { (char *)"/system/bin/pm", disable ? (char *)"disable" : (char *)"enable", (char *)svc, NULL };
  mt_run(argv, 1);
}


static int walk_gmslist(const char *path, const char *conf, const char *filter_cat, int force_enable_all) {
  FILE *f = fopen(path, "r");
  if (!f) { mt_log(3, "gms: gmslist not found at %s", path); return 0; }
  char line[MT_LINE_MAX];
  int fail = 0;
  while (fgets(line, sizeof(line), f)) {
    size_t n = strcspn(line, "\r\n");
    line[n] = '\0';
    if (line[0] == '\0' || line[0] == '#') continue;
    char *bar = strchr(line, '|');
    if (!bar) continue;
    *bar = '\0';
    const char *svc = line, *cat = bar + 1;
    if (svc[0] == '\0' || cat[0] == '\0') continue;
    if (filter_cat && strcmp(cat, filter_cat) != 0) continue;
    int disable = force_enable_all ? 0 : gms_should_disable(conf, cat);
    char *argv[] = { (char *)"/system/bin/pm", disable ? (char *)"disable" : (char *)"enable", (char *)svc, NULL };
    if (mt_run(argv, 1) != 0) fail++;
  }
  fclose(f);
  return fail;
}

static void droidguard(int on) {
  const char *svcs[] = {
    "com.google.android.gms/com.google.android.gms.droidguard.DroidGuardService",
    "com.google.android.gms/com.google.android.gms.droidguard.DroidGuardGcmTaskService",
    NULL
  };
  for (int i = 0; svcs[i]; i++) pm_toggle(svcs[i], on);
  mt_log(1, on ? "droidguard: disabled (breaks SafetyNet/Play Integrity - banking, Wallet, Play Store checks)"
               : "droidguard: re-enabled");
}

static const char *GMS_LOG_KEYS[] = {
  "gmscorestat_enabled","play_store_panel_logging_enabled","clearcut_events","clearcut_gcm",
  "ga_collection_enabled","clearcut_enabled","analytics_enabled","uploading_enabled",
  "bug_report_in_power_menu","usage_stats_enabled","usagestats_collection_enabled", NULL
};

static void settings_put(const char *ns, const char *key, const char *val) {
  char *argv[] = { (char *)"/system/bin/settings", (char *)"put", (char *)ns, (char *)key, (char *)val, NULL };
  mt_run(argv, 1);
}
static void settings_delete(const char *ns, const char *key) {
  char *argv[] = { (char *)"/system/bin/settings", (char *)"delete", (char *)ns, (char *)key, NULL };
  mt_run(argv, 1);
}

static void gms_log_apply(void) {
  for (int i = 0; GMS_LOG_KEYS[i]; i++) settings_put("global", GMS_LOG_KEYS[i], "0");
  settings_put("global", "phenotype__debug_bypass_phenotype", "1");
  settings_put("global", "phenotype_boot_count", "99");
  settings_put("global", "phenotype_flags", "disable_log_upload=1,disable_log_for_missing_debug_id=1");
  mt_log(1, "gms_log_disable: applied");
}
static void gms_log_revert(void) {
  for (int i = 0; GMS_LOG_KEYS[i]; i++) settings_delete("global", GMS_LOG_KEYS[i]);
  settings_delete("global", "phenotype__debug_bypass_phenotype");
  settings_delete("global", "phenotype_boot_count");
  settings_delete("global", "phenotype_flags");
  mt_log(1, "gms_log_disable: reverted");
}

int main(int argc, char **argv) {
  if (argc < 5) { fprintf(stderr, "usage: gms <conf> <track> <gmslist> early|late|set KEY VALUE|category CAT VALUE\n"); return 2; }
  const char *conf = argv[1], *gmslist = argv[3], *mode = argv[4];
  

  if (strcmp(mode, "early") == 0) return 0; 

  if (strcmp(mode, "late") == 0) {
    if (mt_is_on(conf, "GMS_MASTER")) {
      int fail = walk_gmslist(gmslist, conf, NULL, 0);
      if (fail) mt_log(3, "gms_services: %d service(s) failed to apply", fail);
      mt_log(1, "gms_services: applied per category config");
    } else {
      walk_gmslist(gmslist, conf, NULL, 1); 
    }
    if (mt_is_on(conf, "DISABLE_DROIDGUARD")) droidguard(1);
    if (mt_is_on(conf, "GMS_LOG_DISABLE")) gms_log_apply();
    return 0;
  }

  if (strcmp(mode, "category") == 0) {
    if (argc < 7) { fprintf(stderr, "usage: gms <conf> <track> <gmslist> category CAT VALUE\n"); return 2; }
    const char *cat = argv[5], *val = argv[6];
    if (!mt_is_on(conf, "GMS_MASTER")) return 0;
    int disable = strcmp(val, "1") == 0;
    int fail = 0;
    FILE *f = fopen(gmslist, "r");
    if (!f) { mt_log(3, "gms: gmslist not found at %s", gmslist); return 0; }
    char line[MT_LINE_MAX];
    while (fgets(line, sizeof(line), f)) {
      size_t n = strcspn(line, "\r\n"); line[n] = '\0';
      if (line[0] == '\0' || line[0] == '#') continue;
      char *bar = strchr(line, '|');
      if (!bar) continue;
      *bar = '\0';
      const char *svc = line, *svc_cat = bar + 1;
      if (strcmp(svc_cat, cat) != 0) continue;
      char *a[] = { (char *)"/system/bin/pm", disable ? (char *)"disable" : (char *)"enable", (char *)svc, NULL };
      if (mt_run(a, 1) != 0) fail++;
    }
    fclose(f);
    if (fail) mt_log(3, "gms category %s: %d service(s) failed to apply", cat, fail);
    mt_log(1, "gms category %s: %s", cat, disable ? "disabled" : "enabled");
    return 0;
  }

  if (strcmp(mode, "set") == 0) {
    if (argc < 7) { fprintf(stderr, "usage: gms <conf> <track> <gmslist> set KEY VALUE\n"); return 2; }
    const char *key = argv[5], *val = argv[6];
    int on = strcmp(val, "1") == 0;
    if (strcmp(key, "GMS_MASTER") == 0) {
      if (on) { int fail = walk_gmslist(gmslist, conf, NULL, 0); if (fail) mt_log(3, "gms_services: %d service(s) failed to apply", fail); mt_log(1, "gms_services: applied per category config"); }
      else { walk_gmslist(gmslist, conf, NULL, 1); mt_log(1, "gms_services: all re-enabled (GMS_MASTER off)"); }
    } else if (strcmp(key, "DISABLE_DROIDGUARD") == 0) {
      droidguard(on);
    } else if (strcmp(key, "GMS_LOG_DISABLE") == 0) {
      if (on) gms_log_apply(); else gms_log_revert();
    } else {
      mt_log(2, "gms: unknown key %s", key);
      return 1;
    }
    return 0;
  }

  fprintf(stderr, "gms: unknown mode %s\n", mode);
  return 2;
}
