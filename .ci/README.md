# CI-only scripts

GitHub Actions helpers that are **not** used for local bench / HIL runs. Shared contract checks, OpenAPI tooling, and field smoke live in [`scripts/`](../scripts/README.md).

| Script | Purpose |
|--------|---------|
| [`coverage_native.sh`](coverage_native.sh) | Native unit tests with ≥95% coverage gate (`firmware-native` job). |
| [`ci_prerelease_tag.sh`](ci_prerelease_tag.sh) | Compute pre-release tag from commit SHA (delegates to [`scripts/firmware_version_resolve.py`](../scripts/firmware_version_resolve.py)). |

**Local pre-release-style builds** (same embedded version as GitHub `vX.Y.Z-main.<sha>`, branch slug from current git branch):

```bash
pio run -e wroom32_prerelease
# or: HELIO_ZERO_FIRMWARE_PRERELEASE=1 pio run -e wroom32
# manual: eval "$(python3 scripts/firmware_version_resolve.py --export)" && pio run -e wroom32
```
| [`release_notes_template.sh`](release_notes_template.sh) | Seed GitHub release body. |

Local CI-parity aggregate (native, contract checks, web Vitest, build, optional HIL pytest): [`scripts/run_all_firmware_checks.sh`](../scripts/run_all_firmware_checks.sh).
