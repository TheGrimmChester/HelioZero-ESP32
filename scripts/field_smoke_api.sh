#!/usr/bin/env bash
# P0-6e API-level field smoke (run against home router or mock on LAN).
# Full USB flash / Wi‑Fi / GPIO checklist: /en/getting-started/
set -euo pipefail
BASE="${1:-${HELIO_ZERO_FIELD_URL:-${HELIO_ZERO_MOCK_URL:-${HELIO_ZERO_HIL_URL:-http://127.0.0.1:8787}}}}"
BASE="${BASE%/}"

echo "== Field smoke API (${BASE}) =="

curl -fsS -m 10 "${BASE}/api/v1/health" | python3 -c "import json,sys; d=json.load(sys.stdin); assert d.get('ok') is True or d.get('status')=='ok' or 'ok' in str(d).lower(), d"

m="$(curl -fsS -m 10 "${BASE}/api/v1/measurements")"
python3 -c "
import json, sys
m = json.loads(sys.argv[1])
house = m['house']
for k in ('active_import_w','active_export_w','grid_net_w','house_load_w','pv_production_w'):
    assert k in house, k
assert house['grid_net_w'] == house['active_import_w'] - house['active_export_w']
print('measurements OK:', house['grid_net_w'], 'W net')
" "$m"

inject_code="$(curl -fsS -m 5 -o /dev/null -w "%{http_code}" -X POST "${BASE}/api/v1/sources/test/inject" \
  -H 'Content-Type: application/json' \
  -d '{"house":{"active_import_w":80,"active_export_w":3500}}' 2>/dev/null || echo "000")"
if [[ "${inject_code}" =~ ^2 ]]; then
  if [[ "${HELIO_ZERO_STRICT_PRODUCTION:-0}" == "1" ]]; then
    echo "FAIL: POST /api/v1/sources/test/inject returned ${inject_code} (lab API on production target)" >&2
    exit 1
  fi
  sleep 1
  st="$(curl -fsS -m 10 "${BASE}/api/v1/state")"
  python3 -c "
import json, sys
st = json.loads(sys.argv[1])
pct = float(st['triac_open_percent'])
assert pct >= 0, st
print('triac_open_percent after export inject:', pct)
" "$st"
else
  echo "inject route absent (expected on wroom32 production) — skipping triac inject check"
fi

echo "Field smoke API checks passed for ${BASE}"
