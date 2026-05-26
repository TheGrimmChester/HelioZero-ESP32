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

_SAVED_HIL_URL="${HELIO_ZERO_HIL_URL:-}"
_SAVED_FIELD_URL="${HELIO_ZERO_FIELD_URL:-}"
_SAVED_BEARER="${HELIO_ZERO_API_BEARER_TOKEN:-}"
_SAVED_HIL_PASSWORD="${HELIO_ZERO_HIL_PASSWORD:-}"

echo "== Web contract tests =="
# Lab exports must not leak into vitest (shell BASE / FIELD_URL break path helpers).
unset BASE HELIO_ZERO_FIELD_URL
(cd "${ROOT}/web" && npm ci && npm run typecheck && npm run test:coverage)

echo "== Production firmware build =="
"$PIO" run -e wroom32
"${ROOT}/scripts/check_firmware_flash_size.sh" wroom32

if [[ -n "${_SAVED_HIL_URL}" ]]; then
  export HELIO_ZERO_HIL_URL="${_SAVED_HIL_URL}"
fi
if [[ -n "${_SAVED_FIELD_URL}" ]]; then
  export HELIO_ZERO_FIELD_URL="${_SAVED_FIELD_URL}"
fi
if [[ -n "${_SAVED_BEARER}" ]]; then
  export HELIO_ZERO_API_BEARER_TOKEN="${_SAVED_BEARER}"
fi
if [[ -n "${_SAVED_HIL_PASSWORD}" ]]; then
  export HELIO_ZERO_HIL_PASSWORD="${_SAVED_HIL_PASSWORD}"
fi

HIL_URL="${HELIO_ZERO_HIL_URL:-}"
if [[ -n "${HIL_URL}" ]]; then
  echo "== HIL pytest (${HIL_URL}) =="
  "$PY" -m pip install -q -r firmware/test/hil/requirements.txt
  "$PY" -m pytest firmware/test/hil -q
  echo "== Field smoke API (${HIL_URL}) =="
  "${ROOT}/scripts/field_smoke_api.sh"
else
  echo "== Skipping HIL (set HELIO_ZERO_HIL_URL) =="
fi

echo "All host firmware checks passed."
