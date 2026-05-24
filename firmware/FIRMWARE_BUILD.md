# Firmware build notes

**Field deployment:** [Getting started](https://heliozero.clouded.fr/en/getting-started/) (flash, Wi‑Fi, security, OTA). **Tagged releases:** [Developer guide](https://heliozero.clouded.fr/en/developer/) § Release checklist.

## Stack

- **Primary build**: [PlatformIO](../platformio.ini) with `framework = arduino` and `src_dir = firmware`.
- **Arduino-ESP32 and ESP-IDF**: the Arduino core is built on Espressif’s ESP-IDF. You can call native IDF APIs from `.cpp` files where needed. A full **CMake-first project with Arduino only as an ESP-IDF component** is possible (see [Arduino as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)) but is **not** the default layout here: it increases maintenance cost (toolchain alignment, `WebServer`/Arduino init ordering) without changing runtime behaviour.

## Production vs lab builds

| Env | RemoteDebug | Inject API |
|-----|-------------|------------|
| `wroom32` | off (`-DHELIO_REMOTE_DEBUG=0`) | off |
| `hil` | on | `POST /api/v1/sources/test/inject` |

## Platform / Python

- The default **`wroom32`** environment pins **`platform = espressif32@6.4.0`** so the bundled **esptool** stays compatible with **Python 3.9** (common on macOS Command Line Tools). Newer Espressif32 platform releases may pull **esptool 5.x**, which expects **Python 3.10+**.
- To move to the latest `espressif32` platform, use a **Python 3.10+** virtualenv for PlatformIO and you may remove the `@6.4.0` pin after verifying `pio run` and OTA image size.

## Developer-only Wi‑Fi / MQTT defaults (optional)

Factory defaults are **empty** strings in [`helio_globals.cpp`](core/helio_globals.cpp). For a local lab image only, you can inject non-secret placeholders at compile time (never commit real credentials):

```text
build_flags =
  ${env:wroom32.build_flags}
  '-DHELIO_ZERO_DEFAULT_WIFI_SSID="myssid"'
  '-DHELIO_ZERO_DEFAULT_WIFI_PASSWORD="mypass"'
  '-DHELIO_ZERO_DEFAULT_MQTT_USER="mqtt"'
  '-DHELIO_ZERO_DEFAULT_MQTT_PASSWORD="mqttpass"'
```

Escape quotes as required by your shell; prefer a private `platformio.local.ini` (gitignored) for these lines.

**Never commit** real credentials, `web/.env.local`, or signed fleet JSON with password keys. With [pre-commit](https://pre-commit.com/) installed, Gitleaks runs on each commit (`.gitleaks.toml`; see [CONTRIBUTING.md](../CONTRIBUTING.md)).

## Layout (post-refactor)

| Area | Location |
|------|-----------|
| Entry | [`HelioZero.ino`](HelioZero.ino) → `helio_setup()` / `helio_loop()` |
| App / triac / temperature / energy | [`core/helio_app.cpp`](core/helio_app.cpp), [`core/helio_triac_isr.cpp`](core/helio_triac_isr.cpp) |
| Global state | [`core/helio_globals.cpp`](core/helio_globals.cpp) |
| REST `/api/v1` | [`api_v1_routes.cpp`](api_v1_routes.cpp) + [`api/`](api/) modules |
| Removed pre-v1 HTTP routes | [`http_server.cpp`](http_server.cpp) |
| MQTT HA | [`mqtt_ha.cpp`](mqtt_ha.cpp) |
| SPA routes | [`web_ui_routes.cpp`](web_ui_routes.cpp) |
| Metering | [`metering/*.cpp`](metering/) + [`core/helio_source.cpp`](core/helio_source.cpp) |

## PWA install (Android Chrome)

After `build_web.py`, firmware serves `/manifest.webmanifest`, `/pwa/icon-*.png`, and `/sw.js` from `pageHtmlPwaAssets.h`.

On-device check:

1. Open `http://<router-ip>/` — DevTools Application → manifest `display: fullscreen`, icons from `/pwa/`.
2. Remove any old home-screen shortcut, reinstall (banner **Install** or browser menu).
3. Launch **only from the home screen icon** — URL bar should be hidden.

Regenerate public PWA files: `cd web && npm run sync:brand` (runs `generate-pwa-icons.mjs`).

## Install country table

Regenerate after editing `scripts/generate-install-countries.mjs`:

```bash
node scripts/generate-install-countries.mjs
```

This updates `web/src/data/install-countries.ts` and `firmware/core/helio_install_countries.{h,cpp}`.

## Contract checks

See [`firmware/test/golden/README.md`](test/golden/README.md) and [`scripts/check_firmware_golden.py`](../scripts/check_firmware_golden.py).

## Testing

Solar routing to the cumulus is safety- and comfort-critical: prefer **`./scripts/run_all_firmware_checks.sh`** before release, and run HIL on a bench ESP32 when changing regulation, metering, or triac code.

| Command | Purpose |
|---------|---------|
| `pio test -e native` | Host GoogleTest under [`test/native/`](test/native/) — regulation, meters, inject validation, **full-day replay** (`DayReplayLogic*`) |
| `pio test -e native -f DayReplayLogic` | Full-day replay smoke only |
| `rm -rf .pio && pio run -e wroom32` | After native tests, wipe `.pio` before ESP32 builds (native `platform=native` pollutes the tree; CI uses separate jobs) |
| `python3 scripts/check_openapi_drift.py` | [`openapi/helio-zero-v1.yaml`](../openapi/helio-zero-v1.yaml) vs firmware embed vs `web/test/fixtures/openapi-snapshot.json` |
| `python3 scripts/check_openapi_routes.py` | `api_v1_routes.cpp` (+ onNotFound sub-resources: actions, auth tokens) vs OpenAPI YAML paths |
| `python3 scripts/embed_openapi.py` | Regenerate PROGMEM OpenAPI in `api/api_v1_openapi.cpp` after editing YAML |
| `python3 scripts/sync_openapi_snapshot.py` | Regenerate web OpenAPI fixture from YAML |
| `python3 scripts/check_firmware_golden.py --suite firmware/test/golden/captures` | Per-route JSON key contracts |
| `./scripts/run_all_firmware_checks.sh` | CI-parity host gate: native, MQTT/OpenAPI/golden, web tests, `wroom32` build (+ HIL if `HELIO_ZERO_HIL_URL` set) |
| `pio run -e hil` | Lab firmware with `POST /api/v1/sources/test/inject` |

HIL (hardware): see [`test/hil/README.md`](test/hil/README.md). Set `HELIO_ZERO_HIL_URL`, optional `HELIO_ZERO_HIL_PASSWORD`, then `pytest firmware/test/hil -q`. Inject API accepts optional `sim.wall_decihours` and `sim.temperature_c` for schedule replay without SNTP.

All firmware test assets are indexed in [`test/README.md`](test/README.md).
