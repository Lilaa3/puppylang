let module = null;
let worker = null;
let failedToCreateWorker = false;
let workerReady = false;

let pendingSettings = [];
let pendingOps = [];

const downloads = { encode: { url: null, name: null }, decode: { url: null, name: null } };
const OUT_PATHS = { encode: "/tmp/puplang_enc_out.txt", decode: "/tmp/puplang_dec_out.txt" };
let currentOp = null;

const settingsConfig = {
    "seed": { fn: "puplang_set_seed", type: "bigint", defaultVal: 64 },
    "initial_howl_chance": { fn: "puplang_set_initial_howl_chance", type: "number", defaultVal: 20, percentage: true },
    "howl_decay": { fn: "puplang_set_howl_decay", type: "number", defaultVal: 50, percentage: true },
    "set_free_extension_limit": { fn: "puplang_set_free_extension_limit", type: "number", defaultVal: 0 },
    "min_howl": { fn: "puplang_set_min_howl", type: "number", defaultVal: 10, percentage: true },
    "length_decay_rate": { fn: "puplang_set_length_decay_rate", type: "number", defaultVal: 50, percentage: true },
    "uppercase_weight": { fn: "puplang_set_uppercase_weight", type: "number", defaultVal: 40, percentage: true },
    "max_short_extension": { fn: "puplang_set_max_short_extension", type: "number", defaultVal: 2 },
    "howl_enabled": { fn: "puplang_set_howl_enabled", type: "checkbox", defaultVal: true, callback: updateHowlOptionsVisibility },
    "punctuation_as_char": { fn: "puplang_set_punct_as_char", type: "checkbox", defaultVal: false }
};

function renderStatusPanels() {
    const template = document.getElementById("status-panel-template");

    ["encode", "decode"].forEach(type => {
        const clone = template.content.cloneNode(true);

        clone.querySelector(".download-area").id = `${type}-download-area`;
        clone.querySelector(".download-btn").id = `${type}-download-btn`;
        clone.querySelector(".progress-area").id = `${type}-progress-area`;
        clone.querySelector(".error-text").id = `${type}-error-text`;
        clone.querySelector(".error-message").id = `${type}-error-message`;

        document.getElementById(`${type}-status`).appendChild(clone);
    });
}

function showError(msg, op) {
    document.getElementById(`${op}-error-text`).textContent = msg;
    document.getElementById(`${op}-error-message`).classList.remove("hidden");
}

function hideError(op) {
    document.getElementById(`${op}-error-message`).classList.add("hidden");
}

function hideErrors() {
    hideError("decode");
    hideError("encode");
}

function updateProgress(type, frac) {
    const pct = Math.max(0, Math.min(100, frac * 100)).toFixed(2);
    const area = document.getElementById(`${type}-progress-area`);
    if (!area) return;
    area.querySelector(".progress-bar").style.width = `${pct}%`;
    area.querySelector(".progress-text").textContent = `${pct}%`;
}

function showProgress(type) {
    const oppositeType = type === "encode" ? "decode" : "encode";
    document.getElementById(`${oppositeType}-progress-area`)?.classList.add("hidden");

    const area = document.getElementById(`${type}-progress-area`);
    if (area) {
        area.classList.remove("hidden");
        updateProgress(type, 0);
    }
}

function hideProgress(type) {
    setTimeout(() => {
        const area = document.getElementById(`${type}-progress-area`);
        if (area) area.classList.add("hidden");
    }, 200);
}

function updateHowlOptionsVisibility() {
    const checked = document.getElementById("howl_enabled").checked;
    document.querySelectorAll(".howl-option").forEach(el => {
        el.classList.toggle("hidden", !checked);
    });
}

// --- Downloads ---
function deriveDownloadName(originName, type) {
    const dot = originName.lastIndexOf(".");
    const base = dot > 0 ? originName.slice(0, dot) : originName;
    const ext = dot > 0 ? originName.slice(dot) : "";
    return `${base}-${type}${ext}`;
}

function showDownload(type, fileName, blob) {
    const isEncode = type === "encode";

    if (downloads[type].url) {
        URL.revokeObjectURL(downloads[type].url);
    }
    const newUrl = URL.createObjectURL(blob);
    downloads[type] = {
        url: newUrl,
        name: deriveDownloadName(fileName, type)
    };

    document.querySelector(`#${type}-download-btn span`).textContent = `Download ${fileName}`;
    document.getElementById(`${type}-download-area`).classList.remove("hidden");
}

function hideDownload(type) {
    if (downloads[type].url) {
        URL.revokeObjectURL(downloads[type].url);
    }
    downloads[type] = { url: null, name: null };
    document.getElementById(`${type}-download-area`).classList.add("hidden");
}

function downloadFromPath(type) {
    const { url, name } = downloads[type];
    if (!url || !name) return;

    const a = document.createElement("a");
    a.href = url;
    a.download = name;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
}

// --- Worker ---
async function createWorker() {
    if (worker) return;
    try {
        worker = new Worker("worker.js");
        worker.onmessage = onWorkerMessage;
        worker.postMessage({ type: "init" });
    }
    catch (ex) {
        showError(`Failed to create worker: ${ex}`, "decode"); // bottom one
        failedToCreateWorker = true; // permanently fail
    }
}

function sendToWorker(msg, isSetting = false) {
    if (!worker && !failedToCreateWorker) {
        createWorker();
    }
    if (!workerReady) {
        (isSetting ? pendingSettings : pendingOps).push(msg);
        return;
    }
    worker.postMessage(msg);
}

function forwardSetting(fn, argType, val) {
    sendToWorker({ type: "set", fn, argType, val }, true);
}

const workerHandlers = {
    kms: (msg) => {
        showError(msg.msg, msg.op);
        if (msg.ex)
            console.error(msg.ex);
        if (!worker) return;
        worker.terminate();
        worker = null;
        workerReady = false;
        pendingOps = [];
    },
    progress: (msg) => updateProgress(msg.op || currentOp, msg.value),
    ready: () => {
        workerReady = true;
        [...pendingSettings, ...pendingOps].forEach(item => worker.postMessage(item));
        pendingSettings = [];
        pendingOps = [];
    },
    result: (msg) => {
        if (msg.mode === "file") {
            hideProgress(msg.op);

            const blob = new Blob([msg.out], { type: "application/octet-stream" });
            showDownload(msg.op, msg.name, blob);
        } else {
            const targetEl = document.getElementById(msg.op === "encode" ? "woof" : "text");
            targetEl.value = msg.out;
            document.getElementById("text").classList.remove("error");
            document.getElementById("woof").classList.remove("error");
            hideError(msg.op);
        }
    },
    error: (msg) => {
        if (msg.mode === "file") {
            hideProgress(msg.op);
            hideDownload(msg.op);
        }
        showError(msg.msg, msg.op);
        if (msg.ex)
            console.error(msg.ex);
    }
};

function onWorkerMessage(e) {
    workerHandlers[e.data.type]?.(e.data);
}

// --- Operations ---
function processText(op) {
    hideErrors();
    const fieldId = op === "encode" ? "text" : "woof";
    sendToWorker({ type: op, content: document.getElementById(fieldId).value, op, mode: "text" });
}

const encodeText = () => processText("encode");
const decodeText = () => processText("decode");

function updateSettingAndReencode(e, encode = true) {
    const input = e.target;
    const config = settingsConfig[input.id];
    if (!config) return;

    const isCheckbox = config.type === "checkbox";
    const rawVal = isCheckbox ? (input.checked ? 1 : 0)
        : config.type === "bigint" ? BigInt(input.value)
            : Number(input.value);
    const finalVal = isCheckbox ? rawVal : config.percentage ? rawVal / 100 : rawVal;

    forwardSetting(config.fn, config.type, finalVal);

    if (config.callback) config.callback();

    const valSpan = document.getElementById(`${input.id}-value`);
    if (valSpan) valSpan.textContent = `${rawVal}${config.percentage ? "%" : ""}`;
    if (encode) encodeText();
}

function resetToDefaults() {
    document.querySelectorAll("#settings-panel input").forEach(input => {
        if (input.type === "checkbox") {
            input.checked = input.defaultChecked;
        } else {
            input.value = input.defaultValue;
        }
    });
}

function updateSettingsAndReencode() {
    document.querySelectorAll("#settings-panel input").forEach(input => {
        updateSettingAndReencode({ target: input }, false);
    });
}

// --- Events ---
function handleFile(event, op) {
    const file = event.target.files[0];
    if (!file) return;

    currentOp = op;
    hideDownload(op);
    hideErrors();
    showProgress(op);
    sendToWorker({ type: op, file: file, name: file.name, op: op, mode: "file" });
    event.target.value = "";
}

function handleReset() {
    resetToDefaults();
    Object.keys(settingsConfig).forEach(id => {
        const input = document.getElementById(id);
        if (input) input.dispatchEvent(new Event("input", { bubbles: true }));
    });
}

function randomizeSeed() {
    const seedInput = document.getElementById("seed");
    const randomizeBtn = document.getElementById("randomize-seed");

    seedInput.value = Math.floor(Math.random() * 4294967295);
    seedInput.dispatchEvent(new Event("input", { bubbles: true }));

    randomizeBtn.classList.add("pressed");
    setTimeout(() => randomizeBtn.classList.remove("pressed"), 600);
}

function bindEventListeners() {
    document.getElementById("text").addEventListener("input", encodeText);
    document.getElementById("woof").addEventListener("input", decodeText);

    document.querySelectorAll(".hold-button-anim").forEach(btn => {
        btn.addEventListener("click", () => {
            btn.classList.add("pressed");
            setTimeout(() => btn.classList.remove("pressed"), 800);
        });
    });

    document.querySelectorAll(".box").forEach(box => {
        const ta = box.querySelector("textarea");
        box.querySelector(".copy").addEventListener("click", async () => {
            if (ta.value) await navigator.clipboard.writeText(ta.value);
        });
    });

    const settingsToggle = document.getElementById("settings-toggle");
    const settingsPanel = document.getElementById("settings-panel");
    settingsToggle.addEventListener("click", () => {
        const isHidden = settingsPanel.classList.toggle("hidden");
        settingsToggle.classList.toggle("pressed", !isHidden);
    });

    document.getElementById("randomize-seed").addEventListener("click", randomizeSeed);
    document.getElementById("reset-settings").addEventListener("click", handleReset);

    document.querySelectorAll("#settings-panel input").forEach(input => {
        input.addEventListener("input", updateSettingAndReencode);
    });

    const bindFileOption = (type) => {
        const fileInput = document.getElementById(`${type}-file-input`);
        document.getElementById(`${type}-file-btn`).addEventListener("click", () => fileInput.click());
        fileInput.addEventListener("change", (e) => handleFile(e, type));
        document.getElementById(`${type}-download-btn`).addEventListener("click", () => downloadFromPath(type));
    };

    bindFileOption("encode");
    bindFileOption("decode");
}

async function start() {
    renderStatusPanels();
    module = await createPuplangModule();
    createWorker();
    bindEventListeners();
    handleReset();
}

start();