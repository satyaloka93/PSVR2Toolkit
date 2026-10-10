# PSVR2 Toolkit DSX bridge

## Why Toolkit is needed, and when to replace its driver

The stock Sony PlayStation VR2 App driver does not expose the **Toolkit CAPI**
that this bridge uses for Sense adaptive triggers and PCM grip vibration.
First-time users must complete the [base Toolkit installation](https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Installation).
The full Cyberpunk ZIP contains **both our driver and bridge**, but is an add-on
to that installation, not a replacement for its supporting DLLs. Keep
`driver_playstation_vr2_orig.dll`, `libcrossipc.dll`, `libusb-1.0.dll` and
`psvr2_toolkit_capi.dll` beside the driver. The installer refuses an incomplete
base installation instead of overwriting a stock Sony driver with a lone DLL.

Our bundled driver differs from its upstream base in two haptics-related ways:
it adds raw-trigger transport and clears stale grip-PCM attenuation that could
otherwise make grip vibration barely perceptible. It is our tested baseline.
The bridge currently translates effects into **standard CAPI trigger modes**;
raw-trigger support alone is not a reason to replace a working Toolkit driver.

**v0.3.2 does not introduce a new driver.** It includes the identical driver
from v0.2.0/v0.3.1 (SHA-256 below). Existing users can skip driver installation
and update only the bridge and companion. Keep another compatible Toolkit
installation if it works; a different hash is not proof of incompatibility.
Choosing our baseline is a separate, confirmed operation with a backup and
rollback command. The new in-game audio controls require no driver change.

Download [the v0.3.2 full package](https://github.com/satyaloka93/PSVR2Toolkit/releases/tag/cyberpunk-dsx-bridge-v0.3.2)
and read `START_HERE.txt` before setup. DSX from Steam is **not required** and
must not run alongside the bridge.

## Cyberpunk VR Port 0.1.7 setup and in-game audio controls

The updated full package separates `driver/` (optional one-time installation)
from `bridge/` (game setup and per-session launcher). Read `START_HERE.txt`.
Run `bridge/setup_cyberpunk017.cmd` once with the game closed. It previews and
backs up changes: disables EDS UDP autostart, moves only its separate native
RED4ext launcher DLL out of the plugin tree, and installs the **PSVR2 Haptics**
CET companion. It keeps the EDS CET mod, weapon profiles and other settings.
Use `-CheckOnly` to inspect without changes or `-GameRoot` for a manual location.
Driver installation remains an independent, explicitly confirmed operation.

In-game **Mods > DualSense settings > PSVR2 Haptics** provides an audio toggle
and strength slider; F10 is not involved. New installations default to **OFF**,
with a stored strength of **35%** for opt-in. That initial strength is not a
claim of hardware-validated tuning. Preferences persist in
`bin/x64/plugins/cyber_engine_tweaks/mods/PSVR2 Haptics/settings.ini`; the bridge
reads changes approximately once a second. Disabling audio stops WASAPI capture
and clears only its audio queue. Adaptive triggers, semantic weapon recoil,
isolated VR melee pulses and their gains remain independent. Assess melee with
audio off before increasing its strength. Both-grip road rumble from c080162 is
retained when audio is enabled; audio-derived road rumble also stops when that
layer is disabled.

The companion adds a subcategory to the existing `/DS_ATE` Native Settings tab
after EDS registers it. It does not patch EDS files or create another overlay.
Missing/partial settings reads retain the bridge's last complete selection;
on a fresh bridge without menu settings audio defaults off. No driver changes
are required for the menu control itself.

For the upstream-based Cyberpunk VR Port 0.1.7, use
`psvr2_toolkit_dsx_bridge_017.exe` with `run_cyberpunk017.cmd` or
`run_cyberpunk017.ps1`. Start SteamVR and connect both Sense controllers
before the bridge; then start Cyberpunk. The launcher checks for Enhanced
DualSense Support's config and `UDPautostart=false`, refuses a competing DSX
app/UDP client/bridge, and never edits third-party settings. The separate
`Local\CyberpunkVR_PSVR2_Haptics_017_v1` channel carries melee/grip pulses;
the legacy shared mapping's slots 157–160 are game input in 0.1.7 and must
not be interpreted as haptics. A version marker and live heartbeat gate that
channel. The bridge also retains direct monitoring of the mod's weapon trigger
profiles and audio-derived grip haptics.

The DSX app from Steam is **not** required. The bridge accepts the DSX trigger
protocol, but itself owns the Sense effects through PSVR2Toolkit CAPI. You do
need SteamVR, the PlayStation VR2 App, the base Toolkit installation above,
and the Enhanced DualSense Support
Cyberpunk CET mod. The 0.1.7 full ZIP includes the verified raw-trigger Toolkit
driver, its backup-producing installer, the bridge, CAPI loader and launcher.
`START_HERE.txt` gives first-install and existing-driver paths. The included
driver is byte-for-byte the verified v0.2.0 raw-trigger driver (SHA-256
`977856e7c1d54a8aea0bf4ef19095ce901fb658d301ca4ba6bf8e077d83a2b43`).

The 0.1.7 bridge preserves Machine motor vibration at the first auto-fire
transition instead of replacing its sole deduplicated profile command with
OFF. Shot-onset grip pulses and the other resistance-release modes remain.
The isolated protocol/Machine tests exercise the guard and translation. The
release is a Cyberpunk-specific bridge update, not a new Toolkit driver
revision or an Outer Worlds 2 build.

This companion process receives the documented DSX UDP trigger protocol on localhost and translates it to PlayStation VR2 Toolkit CAPI adaptive-trigger commands. It allows game mods that target DSX—such as Cyberpunk 2077 Enhanced DualSense Support—to drive PSVR2 Sense triggers without DSX owning the controllers.

## Legacy Cyberpunk ports only

For **0.1.7**, follow the setup above and use the `_017` executable, not these
older shared-mapping instructions.

Prebuilt bridge and tested driver packages are published by the [PSVR2Toolkit community fork](https://github.com/satyaloka93/PSVR2Toolkit/releases). The bundled driver is a known-tested baseline, not evidence that every other installed Toolkit version needs replacing. The installer skips an identical driver, displays hashes before replacement, requires confirmation and supports restoring its backup. v0.3.1 and v0.3.2 reuse the v0.2.0 driver.

1. Install and start the experimental PSVR2 Toolkit release.
2. Start SteamVR and connect both Sense controllers.
3. Install Cyberpunk 2077 [Enhanced DualSense Support](https://www.nexusmods.com/cyberpunk2077/mods/4156) and its game-mod prerequisites.
4. Run the bridge with the generated config path:
   ```text
   psvr2_toolkit_dsx_bridge.exe --cyberpunk-config "D:\\SteamLibrary\\steamapps\\common\\Cyberpunk 2077\\bin\\x64\\plugins\\cyber_engine_tweaks\\mods\\DualSense Support\\config\\DualSenseXConfig.txt"
   ```
5. In Enhanced DualSense Support, disable **UDP autostart**. Do **not** run DSX or the mod's bundled UDP client. Direct monitoring avoids compatibility problems in the mod's native UDP-client launcher. Do not use its **Restart UDP Client** button; restart the bridge instead.
6. Start Cyberpunk 2077. The bridge logs each translated L2/R2 command and publishes `DSXData.json` status for the mod menu.
7. Stop the bridge with Ctrl+C; it sends `Off` to both triggers before releasing its CAPI slot.

### Weapon profiles

Keep EDS weapon categories at **Default** (`1`) initially. Earlier blanket
Choppy/Soft/Hard recommendations were confounded by competing effect clients
and are withdrawn. The bridge preserves the producer's per-weapon profiles;
the Machine firing-transition fix keeps automatic-trigger vibration active.
This setup update does not retune adaptive-trigger effects or weapon categories.

The bridge can also listen for standard DSX UDP packets on `127.0.0.1`. It uses port `6969` by default and publishes both current and legacy DSX port files so compatible applications can find it. Override this with `--port PORT`.

Release packages include `run_bridge.cmd`, which discovers Cyberpunk through Steam. For an unusual installation location, use:

```powershell
.\run_bridge.ps1 -GameRoot "D:\path\to\Cyberpunk 2077"
```

## Translation fidelity

DSX modes 20–26 correspond to official trigger commands and translate directly. Legacy DSX convenience modes such as `Resistance`, `SemiAutomaticGun`, and `AutomaticGun` also have direct equivalents. DualSense and PSVR2 Sense reuse custom mode numbers `0x22`, `0x23`, and `0x27` but do **not** use the same parameter layouts. Passing DSX's packed Bow force pair directly to Sense can turn an ordinary `4/4` handgun profile into force byte `0x1b`, causing the stiff, chunky response reported in testing.

The Sense-tuned path therefore maps Bow to a weapon-specific gradual slope: light take-up, increasing resistance, a short plateau, and motor release on the shot event. Profile start/end/strength values still distinguish individual weapons. Galloping and Machine use safe official trigger vibration while their timing detail is reproduced by grip PCM rather than unverified raw bytes. `CustomTriggerValue` remains a safe approximation because it manipulates firmware/control flags rather than defining a normal physical effect.

Grip PCM combines two layers:

- **Full-game audio haptics:** Windows WASAPI loopback captures the active default output only while `Cyberpunk2077.exe` is running. A 28–320 Hz tactile band, transient detector, compression, and stereo downsampling convert gunshots, explosions, impacts, vehicles, environmental bass, and other game audio to signed 8-bit 3000 Hz Sense PCM.
- **Semantic effects:** weapon-profile transitions add explicit handgun/revolver recoil, heavy two-hand shotgun impulses, automatic-fire rhythms, charge textures, and trigger-release timing even when the audio mix is quiet.

For the 0.1.7 bridge, use the in-game DualSense audio controls described above.
`--audio-haptics-gain 0..3` is a fallback only when no saved menu selection is
available (0.1.7 default `0`; the legacy executable retains `1.35`). Menu
preferences take priority. `--no-game-audio-haptics` always disables audio;
the CMD/PowerShell wrapper exposes it as `-NoGameAudioHaptics` for diagnostics.
WASAPI is default-output-device loopback, not per-process audio capture; music,
ambience and other applications on that endpoint can contribute. This is not
Cyberpunk's original DualSense waveform. DSX RGB/player LED commands are ignored.

## Troubleshooting

- **CAPI cannot be located:** start SteamVR with Toolkit installed before starting the bridge.
- **UDP bind failed:** close DSX or select a free port with `--port`.
- **No commands appear:** confirm the Cyberpunk mod is enabled and that `--cyberpunk-config` points to its generated `DualSenseXConfig.txt`.
- **No grip haptics:** confirm the bridge prints `Cyberpunk grip PCM haptics enabled`; the installed Toolkit CAPI must expose PCM functions.
- **Cyberpunk's Restart UDP Client button:** do not use it in direct-monitor mode; restart the bridge window instead.
- **Effects remain after a crash:** restart the bridge and exit with Ctrl+C, or restart SteamVR.
