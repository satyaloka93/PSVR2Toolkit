-- Adds to Enhanced DualSense Support's existing Native Settings tab. No edits
-- to EDS, no F10 overlay, no controller-effect or weapon-profile ownership.
local tab = "/DS_ATE"
local category = tab .. "/psvr2Haptics"
local settings = { enabled = false, strength = 35 }
local elapsed = 0
local warned = false

local function save()
    local file, err = io.open("settings.ini", "w")
    if not file then
        print("[PSVR2 Haptics] Cannot save audio settings: " .. tostring(err))
        return false
    end
    local written, writeError = file:write(string.format(
        "Version=1\nAudioEnabled=%d\nAudioStrength=%d\n",
        settings.enabled and 1 or 0, settings.strength))
    local closed, closeError = file:close()
    if not written or not closed then
        print("[PSVR2 Haptics] Cannot save audio settings: " .. tostring(writeError or closeError))
        return false
    end
    return true
end

local function load()
    local file = io.open("settings.ini", "r")
    if not file then
        save() -- First install: audio OFF, 35% strength when opted in.
        return
    end
    local text = file:read("*a")
    file:close()
    local values = {}
    local valid = text ~= nil and #text <= 4096
    for line in string.gmatch(text or "", "[^\r\n]+") do
        local key, value = line:match("^([%a]+)=(%d+)$")
        if not key or values[key] ~= nil then valid = false; break end
        values[key] = tonumber(value)
        if key ~= "Version" and key ~= "AudioEnabled" and key ~= "AudioStrength" then
            valid = false
        end
    end
    if valid and values.Version == 1 and (values.AudioEnabled == 0 or values.AudioEnabled == 1)
        and values.AudioStrength and values.AudioStrength >= 0 and values.AudioStrength <= 300 then
        settings.enabled = values.AudioEnabled == 1
        settings.strength = values.AudioStrength
    else
        print("[PSVR2 Haptics] Invalid settings.ini; audio defaults to OFF. Change an option to save a valid selection.")
    end
end

local function registerMenu()
    local ns = GetMod("nativeSettings")
    if not ns or not ns.pathExists(tab) then return false end
    if ns.pathExists(category) then return true end
    ns.addSubcategory(category, "PSVR2 Haptics", 2)
    local toggle
    toggle = ns.addSwitch(category, "Audio haptics",
        "Optional vibration from the default playback device, including music and ambience. Requires the updated PSVR2Toolkit bridge. Applies within one second. OFF does not disable adaptive triggers, weapon recoil, or VR melee pulses.",
        settings.enabled, false, function(value)
            local old = settings.enabled
            settings.enabled = value
            if not save() then
                settings.enabled = old
                ns.setOption(toggle, old)
            end
        end)
    local slider
    slider = ns.addRangeInt(category, "Audio haptics strength (%)",
        "Strength of audio-derived vibration only. 35% is the initial setting; 0% is silent. Does not change trigger strength or weapon/motion haptics. The bridge reads this setting live.",
        0, 300, 5, settings.strength, 35, function(value)
            local old = settings.strength
            settings.strength = math.max(0, math.min(300, math.floor(value)))
            if not save() then
                settings.strength = old
                ns.setOption(slider, old)
            end
        end)
    print("[PSVR2 Haptics] Audio controls added to the DualSense mod settings page.")
    return true
end

registerForEvent("onInit", function()
    load()
    registerMenu()
end)

registerForEvent("onUpdate", function(delta)
    elapsed = elapsed + delta
    if elapsed < 1 then return end
    elapsed = 0
    -- CET mod initialization order is not guaranteed. Also recover if EDS
    -- reconstructs its tab; never replace that tab or its existing controls.
    if not registerMenu() and not warned then
        print("[PSVR2 Haptics] Waiting for Native Settings UI and the DualSense settings tab.")
        warned = true
    end
end)

registerForEvent("onShutdown", function()
    local ns = GetMod("nativeSettings")
    if ns and ns.pathExists(tab) and ns.pathExists(category) then
        ns.removeSubcategory(category)
    end
end)
