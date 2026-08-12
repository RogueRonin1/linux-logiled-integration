# linux-logiled-integration

Game-driven per-key RGB for the **Logitech G915** on Linux — the thing G HUB does on
Windows, rebuilt as a translation layer between the game and the hardware.

Targets **Total War: Warhammer 3** (Windows build under Proton) and **Factorio**
(native Linux build). Host is Bazzite (Fedora Atomic, KDE/Wayland).

> **Status: feasibility proven, daemon not yet built.**
> The hard unknowns have been settled experimentally — see
> [`docs/FINDINGS.md`](docs/FINDINGS.md), which records every experiment and its
> result, including the ones that contradicted the original plan.

---

## What is actually proven

**Hardware.** The G915 exposes **117 addressable LEDs** and a **7×27 key matrix**
through OpenRGB, with zero unmapped LEDs. Full-keyboard frames sustain **60 Hz with no
backpressure**; the project budget is set at 30 Hz for headroom.

**Warhammer 3 has LED integration after all.** The install ships `libled.dll` —
Creative Assembly's own `EMPIRECOMMON::LOGITECH_HARDWARE_ACCESS` layer — and
`Warhammer3.exe` imports it. It references every LogiLED export we need, including
per-key and bitmap calls.

**The full call chain works on Linux.** WH3's real `libled.dll`, driven under Proton,
loads our shim and calls into it:

```json
{"op":"LogiLedInit"}
{"op":"LogiLedSetTargetDevice","target":6}     // LOGI_DEVICETYPE_PERKEY_RGB
{"op":"LogiLedSetLighting","pct":[100,0,0]}
```

**Factorio's mod route works.** `helpers.write_file` from the native Linux build emits
state at a clean 10 Hz for **~33 µs/tick** — about 0.2% of a tick.

**It does not wake the GPU.** The daemon runs a private OpenRGB instance with 2 of
1709 detectors enabled, so nothing but the keyboard is ever probed. The RTX 3090 held
P8 / 210 MHz across a full 60 Hz benchmark.

## Three things the original plan got wrong

1. **Dropping the DLL beside the .exe does nothing.** `libled.dll` resolves the SDK
   *only* from the registry — there is no bare-filename `LoadLibrary` fallback.
2. **`ServerBinary` is a registry _key_, not a value.** It is opened as a subkey and
   its *default* value is read. Setting it as a string value silently fails.
3. **There is an Authenticode gate.** `libled.dll` calls `WinVerifyTrust` and checks
   the publisher is `Logitech Inc`. Wine implements this for real — it verified
   `steam_api64.dll` as "Valve Corp." — so an **unsigned shim is rejected**.

Correct registry form:

```
[HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID\{A6519E67-7632-4375-AFDF-CAA889744403}\ServerBinary]
@="C:\\path\\to\\LogitechLedEnginesWrapper.dll"
```

## Architecture

```
Warhammer 3 (Proton)                     Factorio (native)
   Warhammer3.exe                           control.lua
        |  imports                               |  helpers.write_file
   libled.dll                                    v
        |  registry CLSID -> WinVerifyTrust    script-output/g915/state.json
        v                                        |  inotify
   shim  (signed PE64, /shim)                    |
        |  TCP 127.0.0.1, length-prefixed        |
        +------------------> daemon (/daemon) <--+
                                  |  mapping + effects + rate limit @30Hz
                                  v
                             OpenRGB SDK (G915-only instance)
                                  v
                             G915, 117 LEDs
```

The shim stays dumb: it serialises calls and never blocks the game. All interpretation
lives in the daemon.

## Layout

| path | contents |
|---|---|
| `shim/` | `shim.c` — logging-only LogiLED shim, all 23 exports undecorated |
| `daemon/` | `openrgb-g915-only.json` — restricted detector config |
| `factorio-mod/` | probe mod: verified `script-output` emitter |
| `mapping/` | `g915-leds.json` — dumped LED names + 7×27 matrix (observed, not transcribed) |
| `tools/` | trust probe, cert installer, libled harness, LED dumper, rate benchmark |
| `docs/` | `FINDINGS.md` — every experiment and result |

## Tools

| tool | what it answers |
|---|---|
| `tools/dumpleds.py` | LED names, indices and physical matrix -> `mapping/g915-leds.json` |
| `tools/ratebench.py` | sustained frame rate and backpressure |
| `tools/holdtest.py` | per-key addressing + whether Direct mode holds |
| `tools/trustprobe.c` | what Wine's `WinVerifyTrust` says about a given DLL |
| `tools/installcert.c` | trust the local CA inside a Proton prefix |
| `tools/ledharness.c` | drive WH3's real `libled.dll` **without launching the game** |
| `tools/make-signing-cert.sh` | generate local signing material (gitignored) |

`ledharness.c` is the useful one: it exercises registry → verify → load →
`GetProcAddress` in about a second, so the whole shim path can be tested without a
AAA game launch.

## Building

Toolchain via Homebrew — no rpm-ostree layering, no reboot, no container:

```bash
brew install mingw-w64 osslsigncode
```

```bash
# shim
x86_64-w64-mingw32-gcc -O2 -shared -o LogitechLedEnginesWrapper.dll shim/shim.c

# sign it (required — unsigned is rejected)
./tools/make-signing-cert.sh
osslsigncode sign -certs certs/chain.pem -key certs/code.key \
  -n "Logitech LED Engines Wrapper" -h sha256 \
  -in LogitechLedEnginesWrapper.dll -out LogitechLedEnginesWrapper-signed.dll
```

## Open questions

- **Does Direct mode hold?** Needs eyes on the keyboard — a test pattern is applied and
  waiting.
- **Does WH3 drive lighting during real gameplay?** The plumbing is proven; whether the
  game exercises it in a campaign or battle is not. One instrumented launch settles it.
- Wired vs Lightspeed behaviour (HID++ feature indexes differ between the two).

## Scope note

This is a local compatibility shim for hardware its owner already paid for. The
signing certificate is self-generated, trusted only inside one Proton prefix, and is
never committed. It satisfies a plugin publisher check — it is not a DRM measure and
circumvents no copy protection.

## Credit

Approach follows [`LogiLed2Corsair`](https://github.com/antonpup/LogiLed2Corsair) —
same trick, different target hardware.
