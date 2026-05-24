# Contributing to HelioZero-ESP32

## Before you open a PR

Run the local CI-parity bundle (requires [ripgrep](https://github.com/BurntSushi/ripgrep), PlatformIO, Node 22):

```bash
./scripts/run_all_firmware_checks.sh
```

Fast path (skips native gcovr coverage gate):

```bash
./scripts/ci_host_checks.sh --skip-coverage
cd web && npm ci && npm run typecheck && npm run test:coverage
pio run -e wroom32 && ./scripts/check_firmware_flash_size.sh wroom32
```

Recommended [pre-commit](https://pre-commit.com/): `pre-commit install` then each commit runs [Gitleaks](https://github.com/gitleaks/gitleaks) (`.gitleaks.toml`), naming and asset checks, and web typecheck.

## CI on GitHub

| Workflow | What it gates |
|----------|----------------|
| [Firmware](.github/workflows/firmware.yml) | `ci_host_checks.sh`, coverage, builds, web typecheck + Vitest 95%, optional HIL |
| [Release](.github/workflows/release.yml) | Same host checks before release artifacts |

## Secrets and credentials

Never commit:

- `web/.env.local`, `platformio.local.ini`
- Real Wi‑Fi, MQTT, or API passwords in source or `build_flags`
- Fleet bundles containing password fields (blocked at runtime too)

Use gitignored `platformio.local.ini` for lab-only `HELIO_ZERO_DEFAULT_*` compile flags — see [firmware/FIRMWARE_BUILD.md](firmware/FIRMWARE_BUILD.md).

## Documentation

Product guides are published at [heliozero.clouded.fr](https://heliozero.clouded.fr/) (EN/FR); source markdown lives in [HelioZero-Website](https://github.com/TheGrimmChester/HelioZero-Website) (`content/en/`, `content/fr/`).

Field-help markdown is generated from SPA locale tables plus website-only examples:

- Summaries (in-app `?` popover): `web/src/i18n/locales/fieldHelp.{en,fr}.ts`
- Examples (docs site only): `web/scripts/field-help-extras/{en,fr}.ts` — **add an entry for every new field-help key** in both languages

```bash
cd web && npx tsx scripts/generate-field-help-docs.ts
# or sync to Website when sibling repo exists:
cd web && npx tsx scripts/generate-field-help-docs.ts --out ../HelioZero-Website/content
```

Output: `web/.field-help-docs/` (gitignored) or Website `content/*/field-help/`. Parity (keys, headings, example blocks) is checked by `web/test/field-help-docs-parity.test.ts`.

## Assets

- **Canonical brand SVGs:** `assets/brand/` only (inlined into firmware via `web/src/brand/brandAssets.ts`).
- **Do not commit** `web/public/` — regenerated on `npm run build` (`sync-brand-assets.mjs`, `generate-pwa-icons.mjs`).
- Wiring emulator icons belong in [HelioZero-Emulator](https://github.com/TheGrimmChester/HelioZero-Emulator), not this repo.

## Developer guide

Build, API, OpenAPI, release checklist: [Developer guide](https://heliozero.clouded.fr/en/developer/) (source: Website `content/en/developer.md`).
