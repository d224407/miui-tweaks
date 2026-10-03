const MODDIR = "/data/adb/modules/miui_tweaks";
const CONF = MODDIR + "/config/tweaks.conf";
const LOAD = MODDIR + "/common/load.sh";
const LOGFILE = "/storage/emulated/0/Android/miui_tweaks.log";
const SOURCE_LOAD = "MODDIR='" + MODDIR + "'; . '" + LOAD + "'";

const DEFAULTS = {
  MIUI_SERVICES: "1", MISC_KILL_SERVICES: "0",
  SYS_LOG_PROPS: "1", SYS_DALVIK_PROPS: "1",
  CPU_PIN: "0", CPU_CORE_HARDCODE: "0", FIXED_PERF_MODE: "0", THERMAL_OVERRIDE: "0",
  PACKAGES_DEXOPT: "0", CMD_MISC: "1",
  LMK_PROPS: "1", TOMBSTONE_DISABLE: "0", BLUR_DISABLE: "0",
  GMS_MASTER: "1", GMS_LOG_DISABLE: "1", DISABLE_DROIDGUARD: "0",
  DISABLE_ADS: "1", DISABLE_TRACKING: "1", DISABLE_ANALYTICS: "1", DISABLE_REPORTING: "1",
  DISABLE_BACKGROUND: "0", DISABLE_UPDATE: "0", DISABLE_LOCATION: "0", DISABLE_GEOFENCE: "0",
  DISABLE_NEARBY: "0", DISABLE_CAST: "0", DISABLE_DISCOVERY: "0", DISABLE_SYNC: "0",
  DISABLE_CLOUD: "0", DISABLE_AUTH: "0", DISABLE_WALLET: "0", DISABLE_PAYMENT: "0",
  DISABLE_WEAR: "0", DISABLE_FITNESS: "0",
  WIFI_QCOM_FIX: "0", WIFI_BAND_CAPABILITY: "0",
  WIFI_KEY_ARP: "1", WIFI_KEY_NS: "1", WIFI_KEY_MCADDR: "1", WIFI_KEY_POWERSAVE: "1",
  WIFI_KEY_RUNTIMEPM: "1", WIFI_KEY_ROAM: "1", WIFI_KEY_11D: "1", WIFI_KEY_RTS: "1",
  WIFI_KEY_SCANTIME: "1", WIFI_KEY_SESSIONS: "1", WIFI_KEY_WAKELOCK: "1",
  SYSBIN_MASTER: "0", STUB_LOG: "0", STUB_TRACED: "0", STUB_DEBUG: "0", STUB_BUGREPORT: "0", STUB_NETDIAG: "0",
  LEGACY_MODE: "0"
};

const SELECT_OPTIONS = {
  WIFI_BAND_CAPABILITY: [
    { value: "0", label: "Auto" }, { value: "1", label: "2.4GHz" }, { value: "2", label: "5GHz" },
  ],
};

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
  { title: "GMS", group: true, masterKey: "GMS_MASTER",
    masterLabel: "GMS service categories", masterDesc: "Tap to choose which categories are disabled",
    applyFns: ["tweak_gms_services"],
    children: [
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
    standalone: [
      ["GMS_LOG_DISABLE", "Disable GMS logging/telemetry", "clearcut, phenotype, analytics, usage-stats Settings.Global flags"],
      ["DISABLE_DROIDGUARD", "Disable DroidGuard", "Breaks SafetyNet/Play Integrity - banking apps, Google Wallet, Play Store integrity checks will fail", true],
    ],
  },
  { title: "Wi-Fi (Qualcomm)", group: true, masterKey: "WIFI_QCOM_FIX",
    masterLabel: "Fix Wi-Fi wakelock drain", masterDesc: "Patches WCNSS_qcom_cfg.ini via mount overlay, needs reboot - tap to choose which keys are written",
    applyFns: ["tweak_wifi_qcom_fix"],
    children: [
      ["WIFI_KEY_ARP", "ARP offload off (hostArpOffload)", "Lets the Wi-Fi chip's firmware answer ARP requests on its own instead of waking the CPU for every one - the single biggest qcom_rx_wakelock reduction"],
      ["WIFI_KEY_NS", "Neighbor Solicitation offload off (hostNsOffload)", "Same idea as ARP offload but for IPv6 Neighbor Discovery - wakes the CPU less on IPv6 networks"],
      ["WIFI_KEY_WAKELOCK", "rx_wakelock_timeout = 0", "Stops the Wi-Fi driver from holding a wakelock after every received packet"],
      ["WIFI_KEY_MCADDR", "Multicast address filtering", "gMCAddrListEnable - drops multicast traffic not explicitly subscribed to, less CPU wake for noisy LANs"],
      ["WIFI_KEY_POWERSAVE", "Max power-save offload", "gEnablePowerSaveOffload = 5, hands more power-state decisions to the Wi-Fi firmware"],
      ["WIFI_KEY_RUNTIMEPM", "Runtime power management", "gRuntimePM - lets the kernel suspend the Wi-Fi chip between bursts of traffic"],
      ["WIFI_KEY_ROAM", "Roaming RSSI threshold", "RoamRssiDiff = 3 - requires a bigger signal gap before switching access points, fewer re-associations"],
      ["WIFI_KEY_11D", "802.11d off", "g11dSupportEnabled - skips reading regulatory/country info from each AP on connect"],
      ["WIFI_KEY_RTS", "RTS threshold raised", "RTSThreshold = 1048576 - effectively disables RTS/CTS handshaking on typical frame sizes"],
      ["WIFI_KEY_SCANTIME", "Scan channel timing", "gActiveMaxChannelTime/gActiveMinChannelTime - shorter active-scan dwell time per channel"],
      ["WIFI_KEY_SESSIONS", "Concurrent session limit", "gMaxConcurrentActiveSessions = 2"],
    ],
    standalone: [
      ["WIFI_BAND_CAPABILITY", "Force Wi-Fi band", "Locks the radio to one band instead of switching automatically. Only applies while the fix above is on.", false, "select"],
    ],
  },
  { title: "System binaries (mount)", group: true, masterKey: "SYSBIN_MASTER",
    masterLabel: "System log/debug binary stubs", masterDesc: "Replaces /system/bin tools with no-ops via mount overlay, needs reboot - tap to choose which",
    applyFns: ["tweak_sysbin_stubs"],
    children: [
      ["STUB_LOG", "Logging (logd, logcat...)", "Disables the system log buffer entirely - logcat, ADB logging, and this module's own log all stop working", true],
      ["STUB_TRACED", "Tracing (traced, atrace...)", "Perfetto/systrace profiling daemons - dev tool only, low impact", true],
      ["STUB_DEBUG", "Crash handling (debuggerd, tombstoned...)", "Native crashes stop generating tombstones or being handled normally", true],
      ["STUB_BUGREPORT", "Bug reports (dumpstate, bugreport...)", "Breaks Settings > Take bug report and related dump tools", true],
      ["STUB_NETDIAG", "Network diagnostics (tcpdump, traceroute...)", "Command-line tools only, rarely used directly - lowest impact of this group", true],
    ],
  },
  { title: "Legacy (deep tweak set)", items: [
    ["LEGACY_MODE", "GhostGMS Legacy deep tweaks", "~78 extra sysprops (logging, BT codec, GPU composition, sleep mode) - broader and less tested than the tweaks above", true],
  ]},
];

let state = {};
let currentTab = "all";
let currentQuery = "";
let openSection = null;

function execCommand(cmd) {
  return new Promise((resolve) => {
    const cb = "cb_" + Date.now() + "_" + Math.floor(Math.random() * 1e6);
    window[cb] = (errno, stdout, stderr) => { resolve({ errno, stdout, stderr }); delete window[cb]; };
    if (window.ksu && window.ksu.exec) { ksu.exec(cmd, "{}", cb); }
    else { resolve({ errno: -1, stdout: "", stderr: "ksu.exec bridge not available" }); }
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

function allItems() {
  const out = [];
  SECTIONS.forEach((s) => {
    if (s.group) {
      s.children.forEach((it) => out.push({ section: s, key: it[0], label: it[1], desc: it[2], risky: !!it[3], type: "switch" }));
      (s.standalone || []).forEach((it) => out.push({ section: s, key: it[0], label: it[1], desc: it[2], risky: !!it[3], type: it[4] || "switch" }));
    } else {
      s.items.forEach((it) => out.push({ section: s, key: it[0], label: it[1], desc: it[2], risky: !!it[3], type: it[4] || "switch" }));
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

function rowHtml(it) {
  const infoHtml =
    '<div class="tweak-info"><span class="tweak-name">' + it.label + (it.risky ? '<span class="tweak-tag">risky</span>' : "") + "</span>" +
    (it.desc ? '<div class="tweak-desc">' + it.desc + "</div>" : "") + "</div>";
  if (it.type === "select") {
    const opts = SELECT_OPTIONS[it.key] || [];
    const cur = state[it.key] || opts[0].value;
    const btns = opts.map((o) => '<button class="' + (o.value === cur ? "active" : "") + '" data-key="' + it.key + '" data-value="' + o.value + '">' + o.label + "</button>").join("");
    return '<div class="tweak-row" data-key="' + it.key + '">' + infoHtml + '<div class="mini-select">' + btns + "</div></div>";
  }
  const on = state[it.key] === "1";
  return '<div class="tweak-row" data-key="' + it.key + '">' + infoHtml + '<div class="switch' + (on ? " on" : "") + '" data-key="' + it.key + '"><div class="thumb"></div></div></div>';
}

function render() {
  const root = document.getElementById("tweakList");
  const items = allItems().filter((it) => matchesTab(it) && matchesQuery(it));
  document.getElementById("enabledCount").textContent = allItems().filter((it) => it.type === "switch" && state[it.key] === "1").length;

  if (items.length === 0) {
    root.innerHTML = '<div class="state-view"><div class="title">No matching tweaks</div><div class="subtitle">Try a different filter or search term</div></div>';
    return;
  }

  const bySection = new Map();
  items.forEach((it) => { if (!bySection.has(it.section)) bySection.set(it.section, []); bySection.get(it.section).push(it); });

  const html = [];
  SECTIONS.forEach((s) => {
    const present = bySection.get(s);
    if (!present) return;
    html.push('<div class="list-title">' + s.title + "</div>");

    if (s.group) {
      const masterOn = state[s.masterKey] === "1";
      html.push(
        '<div class="list-container"><div class="tweak-row master-row" data-group="' + s.title + '">' +
          '<div class="tweak-info"><span class="tweak-name">' + s.masterLabel + "</span>" +
          '<div class="tweak-desc">' + s.masterDesc + "</div></div>" +
          '<div class="switch' + (masterOn ? " on" : "") + '" data-key="' + s.masterKey + '"><div class="thumb"></div></div>' +
        "</div></div>"
      );
      const standaloneItems = present.filter((it) => (s.standalone || []).some((x) => x[0] === it.key));
      if (standaloneItems.length) html.push('<div class="list-container">' + standaloneItems.map(rowHtml).join("") + "</div>");
    } else {
      html.push('<div class="list-container">' + present.map(rowHtml).join("") + "</div>");
    }
  });
  root.innerHTML = html.join("");

  root.querySelectorAll(".master-row").forEach((row) => {
    row.addEventListener("click", () => openSubpage(SECTIONS.find((s) => s.title === row.getAttribute("data-group"))));
  });
  root.querySelectorAll(".switch[data-key]").forEach((sw) => {
    sw.addEventListener("click", (e) => { e.stopPropagation(); const k = sw.getAttribute("data-key"); setKey(k, state[k] === "1" ? "0" : "1"); });
  });
  root.querySelectorAll(".mini-select button").forEach((btn) => {
    btn.addEventListener("click", (e) => { e.stopPropagation(); setKey(btn.getAttribute("data-key"), btn.getAttribute("data-value")); });
  });
}

function showSnackbar(text) {
  const old = document.querySelector(".snackbar");
  if (old) old.remove();
  const el = document.createElement("div");
  el.className = "snackbar"; el.textContent = text;
  document.body.appendChild(el);
  setTimeout(() => el.remove(), 2500);
}

async function loadState() {
  const r = await execCommand("cat " + CONF);
  if (r.stdout) { state = parseConf(r.stdout); render(); }
  else document.getElementById("tweakList").innerHTML = '<div class="state-view"><div class="title">Could not read config</div><div class="subtitle">' + (r.stderr || "") + "</div></div>";
}

async function setKey(key, val) {
  state[key] = val;
  render();
  if (openSection) renderSubpageChildren(openSection);
  const saved = await execCommand("sed -i 's/^" + key + "=.*/" + key + "=" + val + "/' " + CONF);
  if (saved.errno !== 0) { showSnackbar("Failed to save " + key); return; }
  showSnackbar((val === "1" ? "Applying " : "Reverting ") + key + "...");
  const r = await execCommand(SOURCE_LOAD + " && run_single " + key + " " + val);
  showSnackbar(r.errno === 0 ? key + " " + (val === "1" ? "applied" : "reverted") : "Error: " + r.stderr);
}

//////////////////////////////////////////////////////////////////////////
// Subpage: bulk config for a group's children. Master switch is handled
// entirely by setKey() above and never touches children; this subpage
// never touches the master key.
//////////////////////////////////////////////////////////////////////////

function renderSubpageChildren(section) {
  const content = document.getElementById("subpageContent");
  const rows = section.children.map((c) => rowHtml({ key: c[0], label: c[1], desc: c[2], risky: !!c[3], type: "switch" })).join("");
  content.innerHTML =
    '<div class="subpage-bulk-row"><button class="btn" id="bulkNone">Select none</button><button class="btn primary" id="bulkAll">Select all</button></div>' +
    '<div class="list-container">' + rows + "</div>";

  content.querySelectorAll(".switch[data-key]").forEach((sw) => {
    sw.addEventListener("click", () => { const k = sw.getAttribute("data-key"); setKey(k, state[k] === "1" ? "0" : "1"); });
  });
  document.getElementById("bulkAll").addEventListener("click", () => bulkSetGroup(section, "1"));
  document.getElementById("bulkNone").addEventListener("click", () => bulkSetGroup(section, "0"));
}

async function bulkSetGroup(section, val) {
  const keys = section.children.map((c) => c[0]);
  keys.forEach((k) => (state[k] = val));
  render();
  renderSubpageChildren(section);
  const sedChain = keys.map((k) => "sed -i 's/^" + k + "=.*/" + k + "=" + val + "/' " + CONF).join(" && ");
  showSnackbar((val === "1" ? "Selecting all" : "Clearing") + " " + section.masterLabel + "...");
  const saved = await execCommand(sedChain);
  if (saved.errno !== 0) { showSnackbar("Failed to save"); return; }
  const r = await execCommand(SOURCE_LOAD + " && " + section.applyFns.join(" && "));
  showSnackbar(r.errno === 0 ? section.masterLabel + " updated" : "Error: " + r.stderr);
}

function openSubpage(section) {
  openSection = section;
  document.getElementById("subpageTitle").textContent = section.masterLabel;
  renderSubpageChildren(section);
  const el = document.getElementById("subpage");
  el.style.display = "flex";
  requestAnimationFrame(() => el.classList.add("open"));
}
function closeSubpage() {
  const el = document.getElementById("subpage");
  el.classList.remove("open");
  setTimeout(() => { el.style.display = "none"; openSection = null; }, 220);
}
document.getElementById("subpageBack").addEventListener("click", closeSubpage);

async function applyNow() {
  showSnackbar("Applying...");
  const r = await execCommand(SOURCE_LOAD + " && apply_early && apply_late");
  showSnackbar(r.errno === 0 ? "Applied" : "Error: " + r.stderr);
}
async function restoreDefaults() {
  const lines = Object.keys(DEFAULTS).map((k) => "sed -i 's/^" + k + "=.*/" + k + "=" + DEFAULTS[k] + "/' " + CONF).join(" && ");
  showSnackbar("Restoring defaults...");
  const r = await execCommand(lines);
  if (r.errno === 0) { state = { ...DEFAULTS }; render(); showSnackbar("Defaults restored"); }
  else showSnackbar("Error: " + r.stderr);
}
async function showLog() {
  const box = document.getElementById("logbox");
  const r = await execCommand("tail -n 150 " + LOGFILE);
  box.textContent = r.stdout || r.stderr || "(empty)";
}

async function copyLog() {
  const box = document.getElementById("logbox");
  const text = box.textContent || "";
  try {
    await navigator.clipboard.writeText(text);
    showSnackbar("Log copied");
  } catch (e) {
    // Clipboard API can be unavailable in a WebView - fall back to a
    // hidden textarea + execCommand("copy").
    const ta = document.createElement("textarea");
    ta.value = text; ta.style.position = "fixed"; ta.style.opacity = "0";
    document.body.appendChild(ta); ta.focus(); ta.select();
    try { document.execCommand("copy"); showSnackbar("Log copied"); }
    catch (e2) { showSnackbar("Could not copy log"); }
    document.body.removeChild(ta);
  }
}

// Nav bar - same reposition()-the-single-indicator approach as Specter's
// own navigation.ts, trimmed down to 2 tabs and plain show/hide panels
// (no swipe-track - just the tab bar itself).
function wireNavBar() {
  const navTabs = Array.from(document.querySelectorAll(".nav-tab"));
  const indicator = document.getElementById("nav-indicator");

  function reposition(tab) {
    indicator.style.left = tab.offsetLeft + "px";
    indicator.style.width = tab.offsetWidth + "px";
  }

  function switchPanel(name) {
    const tab = navTabs.find((t) => t.getAttribute("data-panel") === name);
    if (!tab) return;
    document.querySelectorAll(".panel").forEach((p) => { p.style.display = p.id === "panel-" + name ? "" : "none"; });
    navTabs.forEach((t) => {
      t.classList.toggle("nav-tab--active", t === tab);
      t.querySelector(".nav-icon").classList.toggle("nav-icon--filled", t === tab);
    });
    reposition(tab);
    if (name === "log") showLog();
  }

  navTabs.forEach((tab) => tab.addEventListener("click", () => switchPanel(tab.getAttribute("data-panel"))));
  window.addEventListener("resize", () => {
    const active = document.querySelector(".nav-tab--active");
    if (active) reposition(active);
  });

  // Indicator starts invisible (0-width) until layout is ready, same as
  // Specter's requestAnimationFrame(() => reposition(homeTab)) on load.
  requestAnimationFrame(() => {
    const active = document.querySelector(".nav-tab--active");
    if (active) reposition(active);
  });
}

document.getElementById("searchInput").addEventListener("input", (e) => { currentQuery = e.target.value; render(); });
document.querySelectorAll(".segmented button").forEach((btn) => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".segmented button").forEach((b) => b.classList.remove("active"));
    btn.classList.add("active"); currentTab = btn.getAttribute("data-tab"); render();
  });
});
document.getElementById("btnApply").addEventListener("click", applyNow);
document.getElementById("btnDefaults").addEventListener("click", restoreDefaults);
document.getElementById("btnRefreshLog").addEventListener("click", showLog);
document.getElementById("btnCopyLog").addEventListener("click", copyLog);
wireNavBar();

loadState();
