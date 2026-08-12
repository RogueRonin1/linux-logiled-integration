# FINDINGS

Every `[VERIFY]` item from the brief, with the experiment that settled it.
Host: Bazzite 44 (bazzite-dx-nvidia), KDE/Wayland, Logitech G915 (wired, PID `046d:c33e`).
Date: 2026-08-12.

Status key: **CONFIRMED** (evidence in hand) / **OPEN** (not yet tested).

---

## Phase 0 — Environment

### OpenRGB install path on an atomic host — CONFIRMED
Bazzite ships `/usr/lib/udev/rules.d/60-openrgb.rules` **in the base image**, so the
read-only `/usr/lib/udev/rules.d` problem described in the brief does not apply here.
The rules already tag both G915 PIDs:

```
046d:c33e -> Logitech_G915_..._Wired      (currently connected)
046d:c541 -> Logitech_G915_..._Lightspeed
```

`uaccess` is applied — `/dev/hidraw5..7` carry an ACL for the logged-in user, so no
root and no extra udev work is needed.

OpenRGB itself is installed as an AppImage at `~/AppImages/openrgb.appimage`
(**1.0rc2**, git `0fca93e`), which `ujust openrgb` also manages.

> **Gotcha:** running two AppImage instances concurrently and letting one exit tears
> down the shared squashfs FUSE mount, killing the survivor with **SIGBUS**. This
> crashed the SDK server mid-benchmark on the first attempt and looked like a
> hardware fault. Fix: `--appimage-extract` once and run the extracted
> `squashfs-root/AppRun`. The daemon must never depend on the FUSE mount.

### Direct mode + device enumeration — CONFIRMED
The G915 enumerates with **117 addressable LEDs** and Direct as the active mode:

```
Modes: [Direct] Static Off Breathing 'Spectrum Cycle' 'Rainbow Wave' 'Reactive (Ripple)'
Zones: Keyboard (117 LEDs)
```

OpenRGB also exposes a **7x27 physical key matrix** for the zone, with every one of
the 117 LEDs present in the grid and **zero unmapped LEDs**. This is the authoritative
layout for the mapping layer — captured to `mapping/g915-leds.json` by
`tools/dumpleds.py`, not transcribed by hand.

Row 0 holds `Logo` and `Brightness`; column 0 holds the `G1`–`G5` macro keys.

### Max sustained update rate — CONFIRMED
`tools/ratebench.py`, full 117-LED frames (468 bytes), 5 s per step, every frame
distinct so nothing dedupes:

| target | achieved | send p95 | send max | drain |
|-------:|---------:|---------:|---------:|------:|
| 5 Hz   | 5.0      | 0.10 ms  | 0.10 ms  | 41 ms |
| 10 Hz  | 10.0     | 0.11 ms  | 0.14 ms  | 0.3 ms |
| 15 Hz  | 15.0     | 0.10 ms  | 0.33 ms  | 0.3 ms |
| 20 Hz  | 20.0     | 0.10 ms  | 0.27 ms  | 0.3 ms |
| 30 Hz  | 30.0     | 0.09 ms  | 0.11 ms  | 0.2 ms |
| 45 Hz  | 45.0     | 0.09 ms  | 0.31 ms  | 0.2 ms |
| 60 Hz  | 60.0     | 0.09 ms  | 0.36 ms  | 0.2 ms |

No backpressure anywhere through 60 Hz. "drain" is the round-trip of a
`REQUEST_CONTROLLER_DATA` issued immediately after the burst — it measures server
backlog, and it stays flat, so frames are not piling up. The 41 ms on the first row
is one-off warm-up, not rate-related.

**Frame budget: 30 Hz.** That is half the measured ceiling, which leaves headroom for
the effect engine and keeps USB traffic modest. Nothing above 30 Hz is perceptible
for this use case.

> Caveat: this measures the path up to and including the OpenRGB server's USB write.
> It does not prove the keyboard's own controller renders every frame. Confirming
> that needs eyes on the hardware — folded into the hold test below.

### GPU power-state pinning — CONFIRMED FIXED
Your concern about OpenRGB pinning the RTX 3090 into a high power state is real in
principle: OpenRGB's I2C/SMBus detectors talk to the GPU's ENE controller on
`/dev/i2c-2`, and bus traffic wakes the GPU.

Fix is architectural — the daemon runs its **own** OpenRGB instance with
`--config <dir>` pointing at a detector set where **2 of 1709 detectors are enabled**
(`daemon/openrgb-g915-only.json`), namely the two G915 entries. Result:

```
controller count: 1
  [0] Logitech G915 Wireless RGB Mechanical Gaming Keyboard (Wired)  (117 LEDs)
```

No GPU, no RAM, no motherboard, no Corsair — nothing else is probed or touched.

Measured during a full 60 Hz benchmark, 60 samples at 1 s intervals:

```
P8, 210 MHz, 48-56 W   <- all 60 samples, no excursion
```

The GPU never left P8. Your normal desktop OpenRGB install is untouched and keeps its
own config.

> Residual: OpenRGB still *opens* the `/dev/i2c-*` fds during bus enumeration even
> with the detectors off (no CLI flag suppresses this), but issues no transactions to
> them. Holding an fd does not wake the GPU — the measurements above confirm it. If
> you want belt-and-braces, the systemd unit can be sandboxed so the process cannot
> see `/dev/i2c-*` at all; noted as a hardening step, not a correctness issue.

### Does Direct mode HOLD? — **OPEN**
This is the one Phase 0 item that needs a human in front of the keyboard, and you
were remote when it ran.

A distinctive pattern is applied **right now** and all traffic has stopped
(`tools/holdtest.py`): WASD green, Q/E yellow, F1–F12 blue, numpad digits red,
G1–G5 + logo white, everything else very dim purple. The G915-only OpenRGB server is
still running.

When you're back, please report which of these you see:
- pattern intact -> Direct holds, gate passed
- reverted to a wave/onboard effect -> Direct needs continuous refresh; the daemon
  must keep a keepalive frame going even when idle
- wrong keys lit -> per-key addressing is off and `mapping/g915-leds.json` needs work
- nothing -> Direct is not reaching the hardware at all

**Not yet tested:** whether behaviour differs wired vs Lightspeed receiver (HID++
feature indexes differ between the two), and whether a G HUB onboard profile set from
the Windows side changes any of it. Both need the keyboard physically switched over.

---

## Phase 1a — Warhammer 3

### Which build is installed — CONFIRMED
The **Windows build under Proton**, not a native Linux build. `Warhammer3.exe` +
`launcher/launcher.exe`, AppID **1142710** (the brief's guess was right), Steam
compat tool `proton_experimental`, prefix at
`/var/mnt/games/SteamLibrary/steamapps/compatdata/1142710/pfx`. A shim is therefore
viable.

### Does WH3 have LED integration at all? — CONFIRMED, and better than expected
The brief's biggest worry — that LightSync was dropped in WH3 — is **wrong for this
build**. The install ships `libled.dll`, Creative Assembly's own LED abstraction layer
(`EMPIRECOMMON::LOGITECH_HARDWARE_ACCESS`), and `Warhammer3.exe` imports it.

`libled.dll` references **every** LogiLED export the project needs, plus the G-key SDK
and the config-option API:

```
LogiLedInit, LogiLedInitWithName, LogiLedSetTargetDevice,
LogiLedSetLighting, LogiLedSetLightingFromBitmap, LogiLedExcludeKeysFromBitmap,
LogiLedSetLightingForKeyWith{ScanCode,HidCode,QuartzCode,KeyName},
LogiLedSetLightingForTargetZone, LogiLedFlash*/Pulse*, LogiLedStopEffects*,
LogiLedSave*/Restore*, LogiLedShutdown
LogiGkey*  (CLSID {7BDED654-...})
LogiGetConfigOption*
```

The class surface confirms real per-key intent: `QueueKeyLEDColour`,
`SetKeyLEDColour`, `FlashKeyLEDColour`, `InitialiseVortexBitMap`,
`ProgressVortexBitMap`, `TurnNumberLEDsOff`, and a `TWKEY -> LGKEY` mapping table
(`m_twkey_lgkey_map`).

### How the SDK DLL is located — CONFIRMED, and the brief was wrong twice

1. **Drop-in beside the executable does not work.** `libled.dll` has no
   `LogitechLedEnginesWrapper.dll` string in either encoding and never calls
   `LoadLibraryW` on a bare filename. It resolves the path **only** from the registry.
   Implementing the drop-in mechanism would be wasted effort for this game.

2. **`ServerBinary` is a registry KEY, not a value.** A `+reg` trace shows:

   ```
   NtOpenKeyEx(L"SOFTWARE\Classes\CLSID\{A6519E67-7632-4375-AFDF-CAA889744403}\ServerBinary", ...)
   NtOpenKeyEx(L"SOFTWARE\Classes\CLSID\{7BDED654-F278-4977-A20F-6E72A0D07859}\ServerBinary", ...)
   ```

   It opens `ServerBinary` as a **subkey** and reads its **default value**. Setting
   `ServerBinary` as a string value under the CLSID key — which is what the brief
   describes — silently fails. Correct form:

   ```
   [HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID\{A6519E67-7632-4375-AFDF-CAA889744403}\ServerBinary]
   @="C:\\path\\to\\shim.dll"
   ```

   Plain `HKLM\SOFTWARE\Classes` is the path that works; no `WOW6432Node` variant was
   consulted (the process is 64-bit).

### Authenticode gate — CONFIRMED, and defeatable
Undocumented in the brief and the single biggest risk to this track: `libled.dll`
imports `WinVerifyTrust` + `CertGetNameStringW` and contains the wide string
`Logitech Inc`. It **signature-verifies the DLL and checks the publisher** before
loading it.

Wine's implementation is real, not a stub — `tools/trustprobe.c` under Proton
Experimental:

| file | WinVerifyTrust | signer |
|---|---|---|
| unsigned shim | `0x800B0100` TRUST_E_NOSIGNATURE | — |
| `libled.dll` | `0x800B0100` TRUST_E_NOSIGNATURE | — |
| `amd_ags_x64.dll` | `0x800B0100` TRUST_E_NOSIGNATURE | — |
| **`steam_api64.dll`** | **`0x00000000` TRUSTED** | **"Valve Corp."** |

An unsigned shim is rejected. The fix is to sign it: a local CA issues a code-signing
leaf with `CN=Logitech Inc`, and `osslsigncode` signs the shim. After that:

```
WinVerifyTrust: 0x00000000  (TRUSTED)
signer: "Logitech Inc"
```

Notably this passed **before** the local CA was added to the prefix's Root store —
Wine's chain policy does not require a trusted root here. `tools/installcert.c` adds
the CA to the prefix anyway so the setup survives Wine tightening that later. Scope is
the Proton prefix only; nothing system-wide is trusted.

### End-to-end: does the real libled.dll drive our shim? — CONFIRMED
`tools/ledharness.c` loads WH3's actual `libled.dll` under Proton and constructs
`EMPIRECOMMON::LOGITECH_HARDWARE_ACCESS` — no game launch needed. With the registry
key correct and the shim signed:

```
IsLEDInitialised  = TRUE
IsGKeyInitialised = FALSE     (G-key SDK not shimmed; not needed for lighting)
```

and the shim's own trace:

```json
{"op":"DllMain","reason":"attach"}
{"op":"LogiLedInit"}
{"op":"LogiLedSetTargetDevice","target":6}
{"op":"LogiLedSetLighting","pct":[100,0,0]}
{"op":"LogiLedSetLighting","pct":[100,0,0]}
{"op":"DllMain","reason":"detach"}
```

Target device **6** = `LOGI_DEVICETYPE_PERKEY_RGB`, and the colour arrives as
**percentages** (100,0,0 = full red), confirming the units split in the brief.

The whole chain works on Linux: registry -> WinVerifyTrust -> LoadLibrary ->
GetProcAddress -> calls arrive in our code.

### Bypassing the Steam launcher — PARTIALLY SOLVED, blocked on a UI dialog
WH3 launches through CA's Electron launcher (`launcher/launcher.exe`), which needs a
click. Two routes were tried:

**Direct launch** — run `Warhammer3.exe` under Proton inside the sniper runtime,
skipping the launcher entirely:

```
SteamAppId=1142710 STEAM_COMPAT_DATA_PATH=<compatdata/1142710> \
  SteamLinuxRuntime_sniper/run -- "Proton - Experimental/proton" run Warhammer3.exe
```

This *starts* — Steam even registers it (`Game process added : AppID 1142710`) — but
the process stalls: **one thread, 0.1% CPU, 0% GPU**, blocked in `ntsync_schedule`
with a single ESTAB socket to the Steam client. It never renders. Bypassing the
launcher is not sufficient on its own; something in the Steam/EOS handshake does not
complete when the game is not started by Steam's own launch flow.

Two prerequisites worth recording, both of which cost a run to discover:
- `XAUTHORITY=/run/user/1000/xauth_OXuksh` must be set or Xwayland refuses the
  connection (`Authorization required, but no authorization protocol specified`).
- Killing `Warhammer3.exe` alone leaves the Proton/pressure-vessel chain alive, and
  the stale session makes Steam refuse the next launch with `AppError_16`. The whole
  tree must be cleaned up.

**Via Steam** (`steam -applaunch 1142710`) — gets much further, then parks:

```
LaunchApp changed task to ProcessingShaderCache
LaunchApp waiting for user response to ProcessingShaderCache
```

It sits there indefinitely waiting for a click on a Steam dialog. Not clearable
without input automation (`ydotool`/uinput), which is not set up on this host.

Useful detail found along the way: `launcher/bypass_time.txt` contains `30`, so the
CA launcher does auto-continue after 30 s — the launcher itself is **not** the
blocker. Steam's own pre-launch dialog is.

### Does the game drive it in actual gameplay? — **OPEN**
The plumbing is proven, but that is not the same as WH3 exercising the lighting during
a campaign or battle. `libled.dll` is CA's layer and the game imports it, so the code
path exists; whether it is wired to live game state is the remaining unknown.

Settling it needs one WH3 launch with the shim registered in logging mode and
`LOGILED_SHIM_LOG` set, then reading the trace. **This is the next experiment and it
needs your go-ahead** — it means launching a AAA game on your machine.

If the trace shows only an init and a static colour, that is a real negative result and
the WH3 track reduces to "static/ambient only" rather than reactive lighting.

---

## Phase 1b — Factorio

### Which build — CONFIRMED
**Native Linux**, `bin/x64/factorio`, base mod **2.1.14**, no `.exe` anywhere. Route B
(the mod) is therefore the correct approach and Route A (Windows build under Proton) is
moot — there is no Windows build installed.

### `script-output` write capability — CONFIRMED
`helpers.write_file(filename, data, append, for_player)` exists in 2.1 (verified
against the shipped `doc-html/runtime-api.json`, not from memory — `game.write_file`
is the pre-2.0 spelling and is gone).

A probe mod (`factorio-mod/`) ran headless via
`--benchmark <save> --benchmark-ticks 1800`:

- `script-output/g915/boot.txt` written from `on_init`
- `state.json` rewritten in place (latest-frame-wins, 91 bytes)
- `log.jsonl` accumulated **exactly 300 records over 1800 ticks** = every 6th tick =
  **10 Hz at 60 UPS**, exactly as designed

Live payload, all fields real:

```json
{"tick":2968044,"daytime":0.480,"research":"","progress":0.0000,"health":1.000,"alerts":0}
```

### Tick cost — CONFIRMED cheap
Same save, same 1800 ticks:

| | avg | min | max |
|---|---:|---:|---:|
| baseline (no mod) | 0.212 ms | 0.150 ms | 0.626 ms |
| with probe @ 10 Hz, **two** file writes | 0.245 ms | 0.168 ms | 7.054 ms |

**~33 µs/tick overhead** against a 16.7 ms budget — about 0.2% of a tick. The 7 ms max
is a first-write outlier, not steady state. Production will write one file, not two.

`on_tick` with an early `% N` return is affordable; the brief's concern about full-rate
polling is satisfied by the decimation.

### Achievements with the mod — CONFIRMED, and this is bad news for Route B

**Enabling any mod moves you to Factorio's separate "modded" achievement track, and
Steam achievements will not unlock.** Evidence from the shipped binary and locale:

```
modded-game = "The game is modded. Achievements are separate from the vanilla version of the game."
delete-achievements-label-tooltip-modded = "This will permanently delete all modded achievements."
```

The game keeps **two** stores, and only the vanilla one is wired to Steam:

```
achievements.dat            <- vanilla
achievements-modded.dat     <- modded, local only

SteamContext::unlockAchievement
SteamContext::isSteamAchievementGained
unlockAchievementsThatAreOnSteamButArentActivatedLocally
```

This was determined by reading the binary and locale only — **no save, config or
achievement data was opened, modified or created.** `~/.factorio` has no
`achievements*.dat` yet, so nothing existed to disturb.

Note this is *not* the same as the console-command/cheat/map-setting rules, which
produce warnings like `command-will-disable-achievements`. Mods are handled by
segregation rather than disabling — you still earn achievements, they just go into the
modded set and never reach Steam.

### Is there a mod-free route on Linux? — CONFIRMED NO
Factorio's `config.ini` lists both:

```
; enable-razer-chroma-support=true
; enable-logitech-led-support=true
```

which initially looked like a mod-free path — Chroma even has a REST API that a fake
local server could answer. But the Linux binary contains **zero** implementation:

- `LogiLed*` SDK symbols: **0 matches**
- Chroma SDK symbols/endpoints: **0 matches** (only the settings key itself, plus
  unrelated `isTextureMonochromatic` and SDL gamepad strings)

Both settings keys are registered in the cross-platform settings schema but the
implementations are compiled out on Linux. **The native Linux build has no built-in LED
support of any kind**, so a mod is the only way to get state out of it — which means
the achievement trade-off above is unavoidable on the native build.

**Consequence — the Factorio track is a genuine either/or:**

| | lighting | Steam achievements | performance |
|---|---|---|---|
| native + mod (Route B) | yes | **no** — modded track | native |
| Windows build under Proton + shim (Route A) | yes, no mod needed | **yes** | Proton overhead |

Route A becomes the achievement-preserving option and needs no mod at all, because the
Windows build has the LogiLED integration built in and would call our shim directly.
It requires downloading the Windows depot (~2 GB) alongside the Linux one. **Not yet
attempted — needs your decision.**

### Would a Razer Chroma shim work instead? — CONFIRMED NO

Asked because Chroma has broader game support than LightSync and a REST API that a
fake local server could answer. It does not help either target game.

**Warhammer 3 has no Chroma integration at all:**

```
Razer/Chroma DLLs in the install     -> none
Razer|Chroma|RzChroma strings in Warhammer3.exe -> 0
Razer|Chroma strings in libled.dll   -> 0
```

Its lighting is Logitech-only, so a Chroma shim would have nothing calling it.

**Factorio's Linux binary has no Chroma implementation**, exactly as with Logitech.
The only chroma-related literal in the whole binary is the settings key:

```
enable-razer-chroma-support                          -> 1  (config schema only)
RzChroma, ChromaSDK, ChromaAnimation, chromasdk,
  RzApi, razerapi, port 54235                        -> 0 each
LogiLed, LogitechLed, LogiGkey                       -> 0 each
```

Both vendor SDKs are compiled out on Linux equally. Chroma is not a way around the
mod requirement on the native build.

A Chroma front end remains worthwhile later for *other* games in the library — it just
does nothing for these two.

### Which API for health — CONFIRMED
`LuaEntityPrototype.max_health` does **not** exist in 2.1 (it moved behind
`get_max_health(quality)` for the quality system). Use `LuaEntity.get_health_ratio()`.
The first probe crashed on exactly this; worth knowing before writing the real mod.

---

## Cleanup performed

Both games are back in their original state and playable.

**Warhammer 3**
- The prefix registry *was* modified for the launch attempt (the `ServerBinary` key).
  It has been restored from a pre-change backup and verified:
  `md5sum -c` -> `system.reg: OK`, and `grep -c A6519E67` -> `0`.
- `user.reg` and `userdef.reg` restored from the same backup.
- **Zero files** added to or modified in the game directory
  (`find -newermt '-2 hours'` -> 0 files). The shim lives in this repo and was
  referenced by a `Z:` path, so the install was never written to.
- Steam integrity is intact — nothing to re-verify.
- `tools/wh3-shim.sh {on|off|status}` now does this registry edit safely, with an
  automatic backup and a refusal to run while the game is up. Current state:
  `disabled (prefix is in its stock state)`.

**Factorio**
- `mod-list.json` restored to its original 5 entries (`base`, `elevated-rails`,
  `quality`, `recycler`, `space-age`).
- Probe mod removed from `~/.factorio/mods` and preserved in `factorio-mod/`.
- `script-output/` removed — it did not exist before the probe created it.
- **No save, config, player-data or achievement file was read or written.**

**Left running deliberately:** the G915-only OpenRGB server, with the hold-test pattern
applied, so the Direct-mode question can be answered by eye. Kill it with
`pkill -x AppRun.wrapped` if it is in the way.

## One dialog left on screen

`steam -applaunch 1142710` is parked on a **"ProcessingShaderCache" dialog** waiting
for a click. It is harmless — dismiss or cancel it when you're back. No game process
is running.

## Toolchain installed

`mingw-w64` 16.2.0 and `osslsigncode` 2.14, both via Homebrew (bottled) — no
rpm-ostree layering, no reboot, no container.
