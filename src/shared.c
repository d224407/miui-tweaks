
#include "common.h"

static const char *LMK[][2] = {
  {"ro.lmk.debug","false"},{"ro.lmk.log_stats","false"},{NULL,NULL}
};
static const char *BLUR[][2] = {
  {"disableBlurs","true"},{"enable_blurs_on_windows","0"},{"ro.launcher.blur.appLaunch","0"},
  {"ro.sf.blurs_are_expensive","0"},{"ro.surface_flinger.supports_background_blur","0"},{NULL,NULL}
};

static void apply_lmk(const char *track, int on) {
  if (on) {
    mt_set_props(track, "LMK_PROPS", LMK);
    mt_log(1, "lmk_props: applied");
  } else {
    mt_revert_props_tag(track, "LMK_PROPS");
  }
}

static void apply_tombstone(const char *track, int on) {
  if (on) {
    mt_set_prop(track, "TOMBSTONE_DISABLE", "tombstoned.max_tombstone_count", "0");
    mt_log(1, "tombstone_disable: applied");
  } else {
    mt_revert_props_tag(track, "TOMBSTONE_DISABLE");
  }
}

static void apply_blur(const char *track, int on) {
  if (on) {
    mt_set_props(track, "BLUR_DISABLE", BLUR);
    mt_log(1, "blur_disable: applied");
  } else {
    mt_revert_props_tag(track, "BLUR_DISABLE");
  }
}

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: shared <conf> <track> early|late|set KEY VALUE\n"); return 2; }
  const char *conf = argv[1], *track = argv[2], *mode = argv[3];

  if (strcmp(mode, "early") == 0 || strcmp(mode, "late") == 0) {
    if (mt_is_on(conf, "LMK_PROPS")) apply_lmk(track, 1);
    if (mt_is_on(conf, "TOMBSTONE_DISABLE")) apply_tombstone(track, 1);
    if (mt_is_on(conf, "BLUR_DISABLE")) apply_blur(track, 1);
    return 0;
  }

  if (strcmp(mode, "set") == 0) {
    if (argc < 6) { fprintf(stderr, "usage: shared <conf> <track> set KEY VALUE\n"); return 2; }
    const char *key = argv[4], *val = argv[5];
    int on = strcmp(val, "1") == 0;
    if (strcmp(key, "LMK_PROPS") == 0) apply_lmk(track, on);
    else if (strcmp(key, "TOMBSTONE_DISABLE") == 0) apply_tombstone(track, on);
    else if (strcmp(key, "BLUR_DISABLE") == 0) apply_blur(track, on);
    else { mt_log(2, "shared: unknown key %s", key); return 1; }
    return 0;
  }

  fprintf(stderr, "shared: unknown mode %s\n", mode);
  return 2;
}
