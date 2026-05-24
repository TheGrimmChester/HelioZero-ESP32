#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
PIO="${ROOT}/.venv/bin/pio"
PY="${ROOT}/.venv/bin/python3"
if [[ ! -x "$PIO" ]]; then PIO=pio; fi
if [[ ! -x "$PY" ]]; then PY=python3; fi

FAST=0
for arg in "$@"; do
  case "$arg" in
    --skip-coverage) FAST=1 ;;
  esac
done

if [[ "$FAST" -eq 1 ]]; then
  "${ROOT}/scripts/ci_host_checks.sh" --skip-coverage
else
  "${ROOT}/scripts/ci_host_checks.sh"
fi

echo "== Web contract tests =="
(cd "${ROOT}/web" && npm ci && npm run typecheck && npm run test:coverage)

echo "== Production firmware build =="
"$PIO" run -e wroom32
"${ROOT}/scripts/check_firmware_flash_size.sh" wroom32

HIL_URL="${HELIO_ZERO_HIL_URL:-}"
if [[ -n "${HIL_URL}" ]]; then
  echo "== HIL pytest (${HIL_URL}) =="
  export HELIO_ZERO_HIL_URL="${HIL_URL}"
  "$PY" -m pip install -q -r firmware/test/hil/requirements.txt
  "$PY" -m pytest firmware/test/hil/test_surplus_routing.py \
    firmware/test/hil/test_inject_regulation.py \
    firmware/test/hil/test_inject_validation.py -q
else
  echo "== Skipping HIL (set HELIO_ZERO_HIL_URL) =="
fi

echo "All host firmware checks passed."
