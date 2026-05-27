# Hardware-in-the-loop (HIL) tests

Pytest suite against a **real ESP32** on the bench (flash `hil` env, poll health, exercise REST).

## Setup

1. Flash: `pio run -e hil -t upload` (from repo root).
2. Set environment variables:
   - `HELIO_ZERO_HIL_URL` — e.g. `http://192.168.1.50`
   - `HELIO_ZERO_HIL_PASSWORD` — optional HTTP API password
   - `HELIO_ZERO_HIL_USER` — optional (default `admin`)

## Run locally

```bash
export HELIO_ZERO_HIL_URL=http://192.168.1.50
pytest firmware/test/hil -q
```

Destructive EEPROM round-trip: `HELIO_ZERO_HIL_EEPROM_ROUNDTRIP=1 pytest firmware/test/hil/test_eeprom_roundtrip.py`

Set `HELIO_ZERO_HIL_REQUIRE_REGULATION=1` to fail instead of skip when the bench device is not regulating.

## CI

Job `firmware-hil` in [`.github/workflows/firmware.yml`](../../.github/workflows/firmware.yml) runs **only when** repository secret `HELIO_ZERO_HIL_URL` is set (checked by job `hil-gate`; self-hosted runner label `esp32-hil`). Without that secret, host jobs (`firmware-native`, `firmware-build`, `web-contract`) still run on every push/PR.

When the HIL job runs, `HELIO_ZERO_HIL_REQUIRE_REGULATION=1` is set so regulation tests **fail** instead of skipping when action 0 is not in AUTO.

Optional secrets: `HELIO_ZERO_HIL_USER`, `HELIO_ZERO_HIL_PASSWORD`.

## Host gate

[`scripts/run_all_firmware_checks.sh`](../../scripts/run_all_firmware_checks.sh) runs native tests, [`scripts/check_firmware_golden.py`](../../scripts/check_firmware_golden.py) (same `required_*_keys.json` as `test_api_smoke`), MQTT/OpenAPI checks, web Vitest, and `wroom32` build. Set `HELIO_ZERO_HIL_URL` to append HIL pytest.

Quick API smoke on the bench (or mock): `HELIO_ZERO_HIL_URL=http://192.168.x.x ./scripts/field_smoke_api.sh`
