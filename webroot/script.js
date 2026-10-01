//////////////////////////////////////////////////////////////////////////
// MIUI Tweaks - WebUI logic
// Reads/writes config/tweaks.conf on-device through the ksu.exec bridge.
//////////////////////////////////////////////////////////////////////////

const MODDIR = "/data/adb/modules/miui_tweaks";
const CONF = MODDIR + "/config/tweaks.conf";
const LOAD = MODDIR + "/common/load.sh";
const LOGFILE = "/storage/emulated/0/Android/miui_tweaks.log";

// engine.sh derives everything from $MODDIR, which is only reliable when
// set explicitly here - a `.`/source does not update $0, so the shell
// running this command has no other way to know the module's path.
const SOURCE_LOAD = "MODDIR='" + MODDIR + "'; . '" + LOAD + "'";

// Must match config/tweaks.conf's shipped defaults.
const DEFAULTS = {
  MIUI_SERVICES: "1", MISC_KILL_SERVICES: "0",
  SYS_LOG_PROPS: "1", SYS_DALVIK_PROPS: "1",
  CPU_PIN: "0", CPU_CORE_HARDCODE: "0", FIXED_PERF_MODE: "0", THERMAL_OVERRIDE: "0",
  PACKAGES_DEXOPT: "0", CMD_MISC: "1",
  LMK_PROPS: "1", TOMBSTONE_DISABLE: "0", BLUR_DISABLE: "0",
  GMS_LOG_DISABLE: "1",
  DISABLE_ADS: "1", DISABLE_TRACKING: "1", DISABLE_ANALYTICS: "1", DISABLE_REPORTING: "1",
  DISABLE_BACKGROUND: "0", DISABLE_UPDATE: "0", DISABLE_LOCATION: "0", DISABLE_GEOFENCE: "0",
  DISABLE_NEARBY: "0", DISABLE_CAST: "0", DISABLE_DISCOVERY: "0", DISABLE_SYNC: "0",
  DISABLE_CLOUD: "0", DISABLE_AUTH: "0", DISABLE_WALLET: "0", DISABLE_PAYMENT: "0",
  DISABLE_WEAR: "0", DISABLE_FITNESS: "0",
  WIFI_QCOM_FIX: "0", WIFI_BAND_CAPABILITY: "0",
  WIFI_KEY_ARP: "1", WIFI_KEY_NS: "1", WIFI_KEY_MCADDR: "1", WIFI_KEY_POWERSAVE: "1",
  WIFI_KEY_RUNTIMEPM: "1", WIFI_KEY_ROAM: "1", WIFI_KEY_11D: "1", WIFI_KEY_RTS: "1",
  WIFI_KEY_SCANTIME: "1", WIFI_KEY_SESSIONS: "1", WIFI_KEY_WAKELOCK: "1",
  LEGACY_MODE: "0"
};

const SELECT_OPTIONS = {
  WIFI_BAND_CAPABILITY: [
    { value: "0", label: "Auto" },
    { value: "1", label: "2.4GHz" },
    { value: "2", label: "5GHz" },
  ],
};

//////////////////////////////////////////////////////////////////////////
// Section catalog. Flat sections: { title, items: [[key,label,desc,risky,type]] }.
// Group sections: { title, group:true, masterKey (or null if computed),
// masterLabel, masterDesc, applyFns:[shell fn names to re-run on bulk/master
// action], children:[[key,label,desc,risky]], extra:[same shape as items,
// rendered after the children, not part of the bulk selection] }.
//////////////////////////////////////////////////////////////////////////

const SECTIONS = [
  { title: "MIUI - Background services", items: [
    ["MIUI_SERVICES", "Disable MIUI bloat services", "Analytics, ads, GameBooster/Joyose and performance daemons"],
    ["MISC_KILL_SERVICES", "Kill background daemons", "statsd, traced, ramdump, cnss_diag, com.miui.daemon - may respawn via watchdog"],
  ]},
  { title: "MIUI - System properties", items: [
    ["SYS_LOG_PROPS", "Disable verbose debug logging", "SurfaceFlinger, codec, IMS, sensors - logging only, no behavior change"],
    ["SYS_DALVIK_PROPS", "ART/Dalvik tuning", "checkjni off, verify-bytecode off, speed-profile filter - JIT stays on"],
  ]},
  { title: "MIUI - CPU / scheduling", items: [
    ["CPU_PIN", "Pin core processes to all performance cores", "zygote, surfaceflinger, system_server at nice -20, continuous battery/heat cost", true],
    ["CPU_CORE_HARDCODE", "Hard-code SurfaceFlinger core assignment", "Assumes an 8-core chip with cores 6-7 as \"big\" - wrong on other layouts", true],
    ["FIXED_PERF_MODE", "Lock CPU/GPU to fixed clocks", "Meant for short benchmarks, drains battery if left running", true],
    ["THERMAL_OVERRIDE", "Disable overheat throttling", "Removes the system's thermal protection entirely - highest risk tweak here", true],
  ]},
  { title: "MIUI - Miscellaneous", items: [
    ["PACKAGES_DEXOPT", "Recompile every installed app", "pm compile speed-profile -a, one-time CPU/battery/storage cost"],
    ["CMD_MISC", "Minor cmd tweaks", "ANR debug overhead, netstats, fstrim interval, dropbox log rate"],
  ]},
  { title: "Shared (MIUI + GMS)", items: [
    ["LMK_PROPS", "Disable Low Memory Killer logging", "ro.lmk.debug / log_stats false"],
    ["TOMBSTONE_DISABLE", "Stop saving crash tombstones", "Off by default - useful for diagnosing a crash if one happens"],
    ["BLUR_DISABLE", "Disable UI blur effects", "Launcher and SurfaceFlinger blur - cosmetic only"],
  ]},
  { title: "GMS", group: true, masterKey: null,
    masterLabel: "GMS tweaks", masterDesc: "Bulk-enable everything below, or bulk-disable your current picks",
    applyFns: ["tweak_gms_services", "tweak_gms_log_disable"],
    children: [
      ["GMS_LOG_DISABLE", "Disable GMS logging/telemetry", "clearcut, phenotype, analytics, usage-stats Settings.Global flags"],
      ["DISABLE_ADS", "Advertising ID service", ""],
      ["DISABLE_TRACKING", "Tracking components", ""],
      ["DISABLE_ANALYTICS", "Analytics / checkin", ""],
      ["DISABLE_REPORTING", "Bug/usage reporting", ""],
      ["DISABLE_BACKGROUND", "Background services", ""],
      ["DISABLE_UPDATE", "Internal GMS auto-update", ""],
      ["DISABLE_LOCATION", "Location reporting + ARCore motion tracking", "Includes HardwareArProviderService (continuous sensor fusion) - may affect background location apps"],
      ["DISABLE_GEOFENCE", "Geofencing", ""],
      ["DISABLE_NEARBY", "Nearby device discovery", ""],
      ["DISABLE_CAST", "Google Cast", ""],
      ["DISABLE_DISCOVERY", "Component discovery / Firebase", "Some apps that embed Firebase may misbehave"],
      ["DISABLE_SYNC", "Account sync", ""],
      ["DISABLE_CLOUD", "Cloud storage", ""],
      ["DISABLE_AUTH", "Secondary auth/account services", "Core auth infrastructure is never disabled", true],
      ["DISABLE_WALLET", "Google Wallet", ""],
      ["DISABLE_PAYMENT", "Payment services", ""],
      ["DISABLE_WEAR", "Wear OS companion", ""],
      ["DISABLE_FITNESS", "Fitness tracking", ""],
    ],
  },
  { title: "Wi-Fi (Qualcomm)", group: true, masterKey: "WIFI_QCOM_FIX",
    masterLabel: "Fix Wi-Fi wakelock drain", masterDesc: "Patches WCNSS_qcom_cfg.ini - needs reboot. Bulk-enable all keys below, or bulk-disable your current picks",
    applyFns: ["tweak_wifi_qcom_fix"],
    children: [
      ["WIFI_KEY_ARP", "ARP offload off", "hostArpOffload - main wakelock fix"],
      ["WIFI_KEY_NS", "Neighbor Solicitation offload off", "hostNsOffload (IPv6) - main wakelock fix"],
      ["WIFI_KEY_WAKELOCK", "rx_wakelock_timeout = 0", ""],
      ["WIFI_KEY_MCADDR", "Multicast address filtering", "gMCAddrListEnable"],
      ["WIFI_KEY_POWERSAVE", "Max power-save offload", "gEnablePowerSaveOffload"],
      ["WIFI_KEY_RUNTIMEPM", "Runtime power management", "gRuntimePM"],
      ["WIFI_KEY_ROAM", "Roaming RSSI threshold", "RoamRssiDiff = 3, fewer AP switches"],
      ["WIFI_KEY_11D", "802.11d off", "g11dSupportEnabled"],
      ["WIFI_KEY_RTS", "RTS threshold raised", "RTSThreshold = 1048576"],
      ["WIFI_KEY_SCANTIME", "Scan channel timing", "gActiveMaxChannelTime / gActiveMinChannelTime"],
      ["WIFI_KEY_SESSIONS", "Concurrent session limit", "gMaxConcurrentActiveSessions = 2"],
    ],
    extra: [
      ["WIFI_BAND_CAPABILITY", "Force Wi-Fi band", "Locks the radio to one band instead of switching automatically. Only applies while the fix above is on.", false, "select"],
    ],
  },
  { title: "Legacy (deep tweak set)", items: [
    ["LEGACY_MODE", "GhostGMS Legacy deep tweaks", "~78 extra sysprops (logging, BT codec, GPU composition, sleep mode) - broader and less tested than the tweaks above", true],
  ]},
];

//////////////////////////////////////////////////////////////////////////
// State
//////////////////////////////////////////////////////////////////////////

let state = {};
let currentTab = "all";
let currentQuery = "";

//////////////////////////////////////////////////////////////////////////
// ksu.exec bridge
//////////////////////////////////////////////////////////////////////////

function execCommand(cmd) {
  return new Promise((resolve) => {
    const cb = "cb_" + Date.now() + "_" + Math.floor(Math.random() * 1e6);
    window[cb] = (errno, stdout, stderr) => {
      resolve({ errno, stdout, stderr });
      delete window[cb];
    };
    if (window.ksu && window.ksu.exec) {
      ksu.exec(cmd, "{}", cb);
    } else {
      resolve({ errno: -1, stdout: "", stderr: "ksu.exec bridge not available - open this page from a root manager app that supports WebUI" });
    }
  });
}

function parseConf(text) {
  const out = {};
  text.split("\n").forEach((line) => {
    line = line.trim();
    if (!line || line.startsWith("#")) return;
    const i = line.indexOf("=");
    if (i === -1) return;
    out[line.slice(0, i).trim()] = line.slice(i + 1).trim();
  });
  return out;
}

//////////////////////////////////////////////////////////////////////////
// Flattened item list (for search/tab filtering across both flat and
// group sections) - each item knows which section/group it belongs to.
//////////////////////////////////////////////////////////////////////////

function allItems() {
  const out = [];
  SECTIONS.forEach((s) => {
    if (s.group) {
      s.children.forEach((it) => out.push({ section: s, scope: "child", key: it[0], label: it[1], desc: it[2], risky: !!it[3], type: "switch" }));
      (s.extra || []).forEach((it) => out.push({ section: s, scope: "extra", key: it[0], label: it[1], desc: it[2], risky: !!it[3], type: it[4] || "switch" }));
    } else {
      s.items.forEach((it) => out.push({ section: s, scope: "flat", key: it[0], label: it[1], desc: it[2], risky: !!it[3], type: it[4] || "switch" }));
    }
  });
  return out;
}

function matchesTab(item) {
  if (currentTab === "on") return item.type === "switch" && state[item.key] === "1";
  if (currentTab === "risky") return item.risky;
  return true;
}
function matchesQuery(item) {
  if (!currentQuery) return true;
  const q = currentQuery.toLowerCase();
  return item.label.toLowerCase().includes(q) || item.key.toLowerCase().includes(q) || item.desc.toLowerCase().includes(q);
}

//////////////////////////////////////////////////////////////////////////
// Rendering
//////////////////////////////////////////////////////////////////////////

function rowHtml(it, extraClass) {
  const infoHtml =
    '<div class="tweak-info">' +
      '<span class="tweak-name">' + it.label + (it.risky ? '<span class="tweak-tag">risky</span>' : "") + "</span>" +
      (it.desc ? '<div class="tweak-desc">' + it.desc + "</div>" : "") +
    "</div>";

  if (it.type === "select") {
    const opts = SELECT_OPTIONS[it.key] || [];
    const cur = state[it.key] || opts[0].value;
    const btns = opts.map((o) =>
      '<button class="' + (o.value === cur ? "active" : "") + '" data-key="' + it.key + '" data-value="' + o.value + '">' + o.label + "</button>"
    ).join("");
    return '<div class="tweak-row ' + (extraClass || "") + '" data-key="' + it.key + '">' + infoHtml + '<div class="mini-select">' + btns + "</div></div>";
  }
  const on = state[it.key] === "1";
  return '<div class="tweak-row ' + (extraClass || "") + '" data-key="' + it.key + '">' + infoHtml +
      '<div class="switch' + (on ? " on" : "") + '" data-key="' + it.key + '"><div class="thumb"></div></div>' +
    "</div>";
}

function render() {
  const root = document.getElementById("tweakList");
  const items = allItems().filter((it) => matchesTab(it) && matchesQuery(it));

  document.getElementById("enabledCount").textContent =
    allItems().filter((it) => it.type === "switch" && state[it.key] === "1").length;

  if (items.length === 0) {
    root.innerHTML = '<div class="state-view"><div class="title">No matching tweaks</div><div class="subtitle">Try a different filter or search term</div></div>';
    return;
  }

  const bySection = new Map();
  items.forEach((it) => {
    if (!bySection.has(it.section)) bySection.set(it.section, []);
    bySection.get(it.section).push(it);
  });

  const html = [];
  SECTIONS.forEach((s) => {
    const present = bySection.get(s);
    if (!present) return;
    html.push('<div class="list-title">' + s.title + "</div>");

    if (s.group) {
      const childKeys = s.children.map((c) => c[0]);
      const onCount = childKeys.filter((k) => state[k] === "1").length;
      const masterOn = s.masterKey ? state[s.masterKey] === "1" : onCount > 0;
      html.push(
        '<div class="list-container">' +
          '<div class="tweak-row master-row' + (masterOn ? " active" : "") + '" data-group="' + s.title + '">' +
            '<div class="tweak-info"><span class="tweak-name">' + s.masterLabel + "</span>" +
            '<div class="tweak-desc">' + s.masterDesc + " (" + onCount + "/" + childKeys.length + " on)</div></div>" +
            '<div class="switch' + (masterOn ? " on" : "") + '"><div class="thumb"></div></div>' +
          "</div>" +
        "</div>"
      );
      const childItems = present.filter((it) => it.scope === "child");
      if (childItems.length) {
        html.push('<div class="list-container group-children">' + childItems.map((it) => rowHtml(it)).join("") + "</div>");
      }
      const extraItems = present.filter((it) => it.scope === "extra");
      if (extraItems.length) {
        html.push('<div class="list-container">' + extraItems.map((it) => rowHtml(it)).join("") + "</div>");
      }
    } else {
      html.push('<div class="list-container">' + present.map((it) => rowHtml(it)).join("") + "</div>");
    }
  });
  root.innerHTML = html.join("");

  root.querySelectorAll(".master-row").forEach((row) => {
    row.addEventListener("click", () => {
      const section = SECTIONS.find((s) => s.title === row.getAttribute("data-group"));
      if (section) toggleGroup(section);
    });
  });
  root.querySelectorAll(".switch[data-key]").forEach((sw) => {
    sw.addEventListener("click", (e) => {
      e.stopPropagation();
      const key = sw.getAttribute("data-key");
      setKey(key, state[key] === "1" ? "0" : "1");
    });
  });
  root.querySelectorAll(".mini-select button").forEach((btn) => {
    btn.addEventListener("click", (e) => {
      e.stopPropagation();
      setKey(btn.getAttribute("data-key"), btn.getAttribute("data-value"));
    });
  });
}

function showSnackbar(text) {
  const old = document.querySelector(".snackbar");
  if (old) old.remove();
  const el = document.createElement("div");
  el.className = "snackbar";
  el.textContent = text;
  document.body.appendChild(el);
  setTimeout(() => el.remove(), 2500);
}

//////////////////////////////////////////////////////////////////////////
// Actions
//////////////////////////////////////////////////////////////////////////

async function loadState() {
  const r = await execCommand("cat " + CONF);
  if (r.stdout) {
    state = parseConf(r.stdout);
    render();
  } else {
    document.getElementById("tweakList").innerHTML =
      '<div class="state-view"><div class="title">Could not read config</div><div class="subtitle">' + (r.stderr || "") + "</div></div>";
  }
}

async function setKey(key, val) {
  state[key] = val;
  render();
  const saveCmd = "sed -i 's/^" + key + "=.*/" + key + "=" + val + "/' " + CONF;
  const saved = await execCommand(saveCmd);
  if (saved.errno !== 0) { showSnackbar("Failed to save " + key); return; }
  showSnackbar((val === "1" ? "Applying " : "Reverting ") + key + "...");
  const r = await execCommand(SOURCE_LOAD + " && run_single " + key + " " + val);
  showSnackbar(r.errno === 0 ? key + " " + (val === "1" ? "applied" : "reverted") : "Error: " + r.stderr);
}

// Master click: if none of the group's children are currently on, turn
// ALL of them on. If one or more are already on, turn off exactly those
// (the user's current picks) rather than forcing the rest on.
async function toggleGroup(section) {
  const childKeys = section.children.map((c) => c[0]);
  const onKeys = childKeys.filter((k) => state[k] === "1");
  const turningOn = onKeys.length === 0;
  const targets = turningOn ? childKeys : onKeys;
  const newVal = turningOn ? "1" : "0";

  targets.forEach((k) => (state[k] = newVal));
  if (section.masterKey) {
    const anyOnAfter = childKeys.some((k) => state[k] === "1");
    state[section.masterKey] = anyOnAfter ? "1" : "0";
  }
  render();

  const sedChain = targets.map((k) => "sed -i 's/^" + k + "=.*/" + k + "=" + newVal + "/' " + CONF).join(" && ");
  const masterSed = section.masterKey ? " && sed -i 's/^" + section.masterKey + "=.*/" + section.masterKey + "=" + state[section.masterKey] + "/' " + CONF : "";
  showSnackbar(turningOn ? "Enabling " + section.masterLabel + "..." : "Disabling selected " + section.masterLabel + "...");
  const saved = await execCommand(sedChain + masterSed);
  if (saved.errno !== 0) { showSnackbar("Failed to save"); return; }
  const r = await execCommand(SOURCE_LOAD + " && " + section.applyFns.join(" && "));
  showSnackbar(r.errno === 0 ? section.masterLabel + " updated" : "Error: " + r.stderr);
}

async function applyNow() {
  showSnackbar("Applying...");
  const r = await execCommand(SOURCE_LOAD + " && apply_early && apply_late");
  showSnackbar(r.errno === 0 ? "Applied" : "Error: " + r.stderr);
}

async function restoreDefaults() {
  const lines = Object.keys(DEFAULTS).map((k) => "sed -i 's/^" + k + "=.*/" + k + "=" + DEFAULTS[k] + "/' " + CONF).join(" && ");
  showSnackbar("Restoring defaults...");
  const r = await execCommand(lines);
  if (r.errno === 0) {
    state = { ...DEFAULTS };
    render();
    showSnackbar("Defaults restored");
  } else {
    showSnackbar("Error: " + r.stderr);
  }
}

async function showLog() {
  const box = document.getElementById("logbox");
  const r = await execCommand("tail -n 100 " + LOGFILE);
  box.textContent = r.stdout || r.stderr || "(empty)";
  box.style.display = "block";
}

//////////////////////////////////////////////////////////////////////////
// Event wiring
//////////////////////////////////////////////////////////////////////////

document.getElementById("searchInput").addEventListener("input", (e) => { currentQuery = e.target.value; render(); });
document.querySelectorAll(".segmented button").forEach((btn) => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".segmented button").forEach((b) => b.classList.remove("active"));
    btn.classList.add("active");
    currentTab = btn.getAttribute("data-tab");
    render();
  });
});
document.getElementById("btnApply").addEventListener("click", applyNow);
document.getElementById("btnDefaults").addEventListener("click", restoreDefaults);
document.getElementById("btnLog").addEventListener("click", showLog);

loadState();
