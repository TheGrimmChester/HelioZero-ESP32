# Scripts

Contract checks, codegen, field bench, and the local CI-parity runner. **CI-only** helpers (coverage gate, release tagging) live in [`.ci/`](../.ci/README.md). PlatformIO pre-hooks: [`extra_scripts/`](../extra_scripts/).

## Contract checks (CI, `run_all_firmware_checks`, golden/HIL)

| Script | Purpose |
|--------|---------|
| [`openapi_paths.py`](openapi_paths.py) | Resolve `openapi/helio-zero-v1.yaml` (optional `HELIO_ZERO_WEBSITE_ROOT`). |
| [`sync_openapi_snapshot.py`](sync_openapi_snapshot.py) | Regenerate `web/test/fixtures/openapi-snapshot.json`. |
| [`embed_openapi.py`](embed_openapi.py) | Embed OpenAPI into `firmware/api/api_v1_openapi.cpp`. |
| [`check_openapi_drift.py`](check_openapi_drift.py) | Fail if YAML, firmware embed, or web fixture diverge. |
| [`check_openapi_routes.py`](check_openapi_routes.py) | Fail if `api_v1_routes.cpp` paths diverge from OpenAPI. |
| [`check_firmware_golden.py`](check_firmware_golden.py) | Validate golden JSON (`firmware/test/golden/`; same contracts as HIL `test_api_smoke`). |
| [`check_mqtt_goldens.py`](check_mqtt_goldens.py) | Validate MQTT golden fixtures. |
| [`check_firmware_flash_size.sh`](check_firmware_flash_size.sh) | Fail if `wroom32` binary exceeds 90% of OTA slot. |
| [`check_naming.sh`](check_naming.sh) | No `rms_*` filenames, product RMS tokens, or camelCase JSON keys in Helio emitters. |
| [`check_tracked_assets.sh`](check_tracked_assets.sh) | Fail if `web/public/**` is git-tracked (generated copies). |
| [`ci_host_checks.sh`](ci_host_checks.sh) | CI-parity bundle: assets, naming, tokens, native, OpenAPI, goldens (`--skip-coverage` optional). |
| [`run_all_firmware_checks.sh`](run_all_firmware_checks.sh) | `ci_host_checks.sh` + web typecheck/coverage + `wroom32` build; with `HELIO_ZERO_HIL_URL`, full HIL pytest + `field_smoke_api.sh`. |

## Codegen

| Script | Purpose |
|--------|---------|
| [`generate-install-countries.mjs`](generate-install-countries.mjs) | Regenerate install-country tables (web + firmware). |
| [`sync-brand-assets.mjs`](sync-brand-assets.mjs) | Copy `assets/brand/` → `web/public/brand/` (`npm run sync:brand`). |

## Field / bench (local, mock, HIL URL)

| Script | Purpose |
|--------|---------|
| [`field_smoke_api.sh`](field_smoke_api.sh) | API smoke — `HELIO_ZERO_FIELD_URL`, `HELIO_ZERO_MOCK_URL`, or `HELIO_ZERO_HIL_URL`; uses `HELIO_ZERO_API_BEARER_TOKEN` or `HELIO_ZERO_HIL_PASSWORD` when the API requires auth. |
| [`fleet_sign_bundle.py`](fleet_sign_bundle.py) | Sign fleet import bundles. |

## Typical runs

```bash
python3 scripts/sync_openapi_snapshot.py
python3 scripts/embed_openapi.py
python3 scripts/check_openapi_drift.py
python3 scripts/check_firmware_golden.py firmware/test/golden/sample_device.json
HELIO_ZERO_HIL_URL=http://192.168.x.x ./scripts/field_smoke_api.sh
./scripts/run_all_firmware_checks.sh
```
