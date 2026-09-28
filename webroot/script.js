//////////////////////////////////////////////////////////////////////////
// MIUI Tweaks - WebUI logic
// Reads/writes config/tweaks.conf on-device through the ksu.exec bridge.
//////////////////////////////////////////////////////////////////////////

const MODDIR = "/data/adb/modules/miui_tweaks";
const CONF = MODDIR + "/config/tweaks.conf";
const TWEAKS = MODDIR + "/common/tweaks.sh";
const LOGFILE = "/storage/emulated/0/Android/miui_tweaks.log";

// Must match config/tweaks.conf's shipped defaults.
const DEFAULTS = {
  MIUI_SERVICES: "1", MISC_KILL_SERVICES: "0",
  SYS_LOG_PROPS: "1", SYS_DALVIK_PROPS: "1",
  CPU_PIN: "0", CPU_CORE_HARDCODE: "0", FIXED_PERF_MODE: "0", THERMAL_OVERRIDE: "0",
  PACKAGES_DEXOPT: "0", CMD_MISC: "1",
  LMK_PROPS: "1", TOMBSTONE_DISABLE: "0", BLUR_DISABLE: "0",
  DISABLE_ADS: "1", DISABLE_TRACKING: "1", DISABLE_ANALYTICS: "1", DISABLE_REPORTING: "1",
  DISABLE_BACKGROUND: "0", DISABLE_UPDATE: "0", DISABLE_LOCATION: "0", DISABLE_GEOFENCE: "0",
  DISABLE_NEARBY: "0", DISABLE_CAST: "0", DISABLE_DISCOVERY: "0", DISABLE_SYNC: "0",
  DISABLE_CLOUD: "0", DISABLE_AUTH: "0", DISABLE_WALLET: "0", DISABLE_PAYMENT: "0",
  DISABLE_WEAR: "0", DISABLE_FITNESS: "0",
  WIFI_QCOM_FIX: "0"
};

//////////////////////////////////////////////////////////////////////////
// Tweak catalog (grouped to match config/tweaks.conf's sections)
//////////////////////////////////////////////////////////////////////////

const GROUPS = [
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
  { title: "Wi-Fi (Qualcomm)", items: [
    ["WIFI_QCOM_FIX", "Fix Wi-Fi wakelock drain", "Patches WCNSS_qcom_cfg.ini via overlay to cut qcom_rx_wakelock wakeups - Qualcomm only, needs reboot"],
  ]},
  { title: "GMS - Service categories", items: [
    ["DISABLE_ADS", "Advertising ID service", ""],
    ["DISABLE_TRACKING", "Tracking components", ""],
    ["DISABLE_ANALYTICS", "Analytics / checkin", ""],
    ["DISABLE_REPORTING", "Bug/usage reporting", ""],
    ["DISABLE_BACKGROUND", "Background services", ""],
    ["DISABLE_UPDATE", "Internal GMS auto-update", ""],
    ["DISABLE_LOCATION", "Location reporting", "May affect apps that rely on background location"],
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
// Filtering + rendering
//////////////////////////////////////////////////////////////////////////

function allItems() {
  return GROUPS.flatMap((g) => g.items.map((it) => ({ group: g.title, key: it[0], label: it[1], desc: it[2], risky: !!it[3] })));
}

function matchesTab(item) {
  if (currentTab === "on") return state[item.key] === "1";
  if (currentTab === "risky") return item.risky;
  return true;
}

function matchesQuery(item) {
  if (!currentQuery) return true;
  const q = currentQuery.toLowerCase();
  return item.label.toLowerCase().includes(q) || item.key.toLowerCase().includes(q) || item.desc.toLowerCase().includes(q);
}

function render() {
  const root = document.getElementById("tweakList");
  const items = allItems().filter((it) => matchesTab(it) && matchesQuery(it));

  document.getElementById("enabledCount").textContent = allItems().filter((it) => state[it.key] === "1").length;

  if (items.length === 0) {
    root.innerHTML = '<div class="state-view"><div class="title">No matching tweaks</div><div class="subtitle">Try a different filter or search term</div></div>';
    return;
  }

  let lastGroup = null;
  const html = [];
  items.forEach((it) => {
    if (it.group !== lastGroup) {
      html.push('<div class="group-title">' + it.group + "</div>");
      lastGroup = it.group;
    }
    const on = state[it.key] === "1";
    html.push(
      '<div class="tweak-row" data-key="' + it.key + '">' +
        '<div class="tweak-info">' +
          '<span class="tweak-name">' + it.label + (it.risky ? '<span class="tweak-tag">risky</span>' : "") + "</span>" +
          (it.desc ? '<div class="tweak-desc">' + it.desc + "</div>" : "") +
        "</div>" +
        '<div class="switch' + (on ? " on" : "") + '" data-key="' + it.key + '"><div class="thumb"></div></div>' +
      "</div>"
    );
  });
  root.innerHTML = html.join("");

  root.querySelectorAll(".switch").forEach((sw) => {
    sw.addEventListener("click", () => {
      const key = sw.getAttribute("data-key");
      const next = state[key] === "1" ? "0" : "1";
      setKey(key, next);
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
  const cmd = "sed -i 's/^" + key + "=.*/" + key + "=" + val + "/' " + CONF;
  const r = await execCommand(cmd);
  showSnackbar(r.errno === 0 ? key + " = " + val : "Failed to save " + key);
}

async function applyNow() {
  showSnackbar("Applying...");
  const cmd = ". " + TWEAKS + " && apply_early && apply_late";
  const r = await execCommand(cmd);
  showSnackbar(r.errno === 0 ? "Applied" : "Error: " + r.stderr);
}

async function restoreDefaults() {
  const lines = Object.keys(DEFAULTS)
    .map((k) => "sed -i 's/^" + k + "=.*/" + k + "=" + DEFAULTS[k] + "/' " + CONF)
    .join(" && ");
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

document.getElementById("searchInput").addEventListener("input", (e) => {
  currentQuery = e.target.value;
  render();
});

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
