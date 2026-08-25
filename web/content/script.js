let module;

const settingsConfig = {
    "seed": { fn: "puplang_set_seed", type: "bigint", defaultVal: 64 },
    "initial_howl_chance": { fn: "puplang_set_initial_howl_chance", type: "number", defaultVal: 20, percentage: true },
    "howl_decay": { fn: "puplang_set_howl_decay", type: "number", defaultVal: 50, percentage: true },
    "set_free_extension_limit": { fn: "puplang_set_free_extension_limit", type: "number", defaultVal: 0 },
    "min_howl": { fn: "puplang_set_min_howl", type: "number", defaultVal: 10, percentage: true },
    "length_decay_rate": { fn: "puplang_set_length_decay_rate", type: "number", defaultVal: 50, percentage: true },
    "uppercase_weight": { fn: "puplang_set_uppercase_weight", type: "number", defaultVal: 40, percentage: true },
    "max_short_extension": { fn: "puplang_set_max_short_extension", type: "number", defaultVal: 2 }
};

const setError = (el, on) => el.classList.toggle("error", on);

function encodeText() {
    const text = document.getElementById("text");
    const woof = document.getElementById("woof");
    const out = module.ccall("puplang_encode", "string", ["string", "string"], [text.value, ""]);
    woof.value = out;
    setError(text, module.ccall("puplang_last_error", "string", [], []) !== "");
    setError(woof, false);
}

function decodeText() {
    const text = document.getElementById("text");
    const woof = document.getElementById("woof");
    const out = module.ccall("puplang_decode", "string", ["string", "string"], [woof.value, ""]);
    const err = module.ccall("puplang_last_error", "string", [], []);
    setError(woof, err !== "");
    setError(text, false);
    if (err === "") {
        text.value = out;
    }
}

function updateHowlOptionsVisibility() {
    const howlToggle = document.getElementById("howl-enabled");
    const howlOptions = document.querySelectorAll(".howl-option");
    if (howlToggle.checked) {
        howlOptions.forEach(el => el.classList.remove("hidden"));
    } else {
        howlOptions.forEach(el => el.classList.add("hidden"));
    }
}

function updateHowlEnabled() {
    const howlToggle = document.getElementById("howl-enabled");
    const enabled = howlToggle.checked;
    module.ccall("puplang_set_howl_enabled", "number", ["bool"], [enabled]);
    updateHowlOptionsVisibility();
    encodeText();
}

function updateSettingAndReencode(e) {
    const input = e.target;
    const config = settingsConfig[input.id];
    if (!config) return;

    const val = config.type === "bigint" ? BigInt(input.value) : Number(input.value);
    let finalVal = val;
    if (config.percentage) {
        finalVal /= 100;
    }
    module.ccall(config.fn, null, [config.type], [finalVal]);

    const valSpan = document.getElementById(`${input.id}-value`);
    if (valSpan) {
        valSpan.textContent = typeof val === "number" ? val.toString() : val.toString();
        if (config.percentage) {
            valSpan.textContent += "%";
        }
    }
    encodeText();
}

function resetToDefaults() {
    const settingsPanel = document.getElementById("settings-panel");
    settingsPanel.querySelectorAll("input").forEach(input => {
        input.value = input.defaultValue;
    });
}

function handleReset() {
    const settingsPanel = document.getElementById("settings-panel");
    resetToDefaults();
    Object.keys(settingsConfig).forEach(id => {
        const input = document.getElementById(id);
        if (!input) return;
        input.dispatchEvent(new Event("input", { bubbles: true }));
    });
    encodeText();
}

function randomizeSeed() {
    const seedInput = document.getElementById("seed");
    const randomizeBtn = document.getElementById("randomize-seed");
    const randomSeed = Math.floor(Math.random() * 4294967295);

    seedInput.value = randomSeed;
    seedInput.dispatchEvent(new Event("input", { bubbles: true }));

    randomizeBtn.classList.add("pressed");
    setTimeout(() => randomizeBtn.classList.remove("pressed"), 600);
}

function bindEventListeners() {
    const text = document.getElementById("text");
    const woof = document.getElementById("woof");
    const settingsToggle = document.getElementById("settings-toggle");
    const settingsPanel = document.getElementById("settings-panel");
    const resetButton = document.getElementById("reset-settings");
    const randomizeBtn = document.getElementById("randomize-seed");
    const howlToggle = document.getElementById("howl-enabled");

    text.addEventListener("input", encodeText);
    woof.addEventListener("input", decodeText);

    document.querySelectorAll(".hold-button-anim").forEach(btn => {
        btn.addEventListener("click", () => {
            console.log(btn);
            btn.classList.add("pressed");
            setTimeout(() => btn.classList.remove("pressed"), 800);
        });
    });

    document.querySelectorAll(".box").forEach(box => {
        const ta = box.querySelector("textarea");
        const btn = box.querySelector(".copy");
        btn.addEventListener("click", async () => {
            const value = ta.value;
            if (!value) return;
            await navigator.clipboard.writeText(value);
        });
    });

    settingsToggle.addEventListener("click", () => {
        const isHidden = settingsPanel.classList.toggle("hidden");
        settingsToggle.classList.toggle("pressed", !isHidden);
    });

    randomizeBtn.addEventListener("click", randomizeSeed);
    howlToggle.addEventListener("change", updateHowlEnabled);

    settingsPanel.querySelectorAll("input").forEach(input => {
        input.addEventListener("input", updateSettingAndReencode);
    });

    resetButton.addEventListener("click", handleReset);
}

async function start() {
    module = await createPuplangModule();

    const howlOptions = document.querySelectorAll(".howl-option");
    console.log(howlOptions);

    bindEventListeners();
    handleReset();
    updateHowlOptionsVisibility();
    encodeText();
}

start();