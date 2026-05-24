# Brand assets (canonical)

SVG logos and favicon used by:

- Repository [`README.md`](../../README.md)
- Web SPA via `web/src/brand/brandAssets.ts` (imports from this folder)
- `web/public/` — **generated** on each `npm run build` (brand SVGs, PWA icons, manifest, service worker); **gitignored** — do not commit.

Edit files here only (`helio-zero-*.svg`). Do not hand-edit or commit duplicates under `web/public/`. CI enforces via [`scripts/check_tracked_assets.sh`](../../scripts/check_tracked_assets.sh).
