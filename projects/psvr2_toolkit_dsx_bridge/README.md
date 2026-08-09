# PSVR2 Toolkit DSX bridge

This companion process receives the documented DSX UDP trigger protocol on localhost and translates it to PlayStation VR2 Toolkit CAPI adaptive-trigger commands. It allows game mods that target DSX—such as Cyberpunk 2077 Enhanced DualSense Support—to drive PSVR2 Sense triggers without DSX owning the controllers.

## Use with Cyberpunk 2077

Prebuilt bridge and matching raw-trigger driver packages are published by the [PSVR2Toolkit community fork](https://github.com/satyaloka93/PSVR2Toolkit/releases). The matching driver is required for native Bow, Galloping, and Machine effects; an older Toolkit driver will not understand the bridge's raw-command transport marker.

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

The bridge can also listen for standard DSX UDP packets on `127.0.0.1`. It uses port `6969` by default and publishes both current and legacy DSX port files so compatible applications can find it. Override this with `--port PORT`.

Release packages include `run_bridge.cmd`, which discovers Cyberpunk through Steam. For an unusual installation location, use:

```powershell
.\run_bridge.ps1 -GameRoot "D:\path\to\Cyberpunk 2077"
```

## Translation fidelity

DSX modes 20–26 correspond to official trigger commands and translate directly. Legacy DSX convenience modes such as `Resistance`, `SemiAutomaticGun`, and `AutomaticGun` also have direct equivalents. Toolkit's raw trigger transport preserves the native Sense command bytes for `Bow` (0x22), `Galloping` (0x23), and `Machine` (0x27), including snap force, alternating amplitudes, frequency, and period. `CustomTriggerValue` remains a safe approximation because it manipulates firmware/control flags rather than defining a normal physical effect.

The bridge also synthesizes grip PCM haptics from Cyberpunk's weapon-effect transitions: overdriven recoil impacts for semi-auto/shotgun transitions and sustained shot-rate textures for automatic, Machine, and Galloping effects. PCM is signed 8-bit at 3000 Hz, and the carrier is intentionally clipped similarly to Toolkit's native OpenVR haptic generator. These are gameplay-derived effects, not Cyberpunk's original DualSense audio waveform. DSX RGB/player LED commands are ignored.

## Troubleshooting

- **CAPI cannot be located:** start SteamVR with Toolkit installed before starting the bridge.
- **UDP bind failed:** close DSX or select a free port with `--port`.
- **No commands appear:** confirm the Cyberpunk mod is enabled and that `--cyberpunk-config` points to its generated `DualSenseXConfig.txt`.
- **No grip haptics:** confirm the bridge prints `Cyberpunk grip PCM haptics enabled`; the installed Toolkit CAPI must expose PCM functions.
- **Cyberpunk's Restart UDP Client button:** do not use it in direct-monitor mode; restart the bridge window instead.
- **Effects remain after a crash:** restart the bridge and exit with Ctrl+C, or restart SteamVR.
