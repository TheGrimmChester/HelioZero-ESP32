"""HIL tests for MQTT/REST parity routes (task010)."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from hil_helpers import get_with_retry

GOLDEN_DIR = Path(__file__).resolve().parents[1] / "golden"


def test_get_telemetry_snapshot_contract(hil_session, hil_base_url):
    r = get_with_retry(hil_session, f"{hil_base_url}/api/v1/telemetry/snapshot", timeout=15)
    assert r.status_code == 200, r.text
    payload = r.json()
    spec = json.loads(
        (GOLDEN_DIR / "required_telemetry_snapshot_keys.json").read_text(encoding="utf-8")
    )
    for key in spec["required_keys"]:
        assert key in payload, f"missing {key} on /api/v1/telemetry/snapshot"


def test_measurements_diagnostics_parity(hil_session, hil_base_url):
    r = get_with_retry(hil_session, f"{hil_base_url}/api/v1/measurements", timeout=15)
    assert r.status_code == 200, r.text
    payload = r.json()
    diag = payload.get("diagnostics") or {}
    assert isinstance(diag, dict)
    for key in ("source_health", "source_stale"):
        assert key in diag, f"missing measurements.diagnostics.{key}"


def test_post_triac_override_auto(hil_session, hil_base_url):
    r = hil_session.post(
        f"{hil_base_url}/api/v1/triac/override",
        json={"command": "AUTO"},
        timeout=15,
    )
    assert r.status_code == 200, r.text
    body = r.json()
    assert body.get("ok") is True


def test_post_triac_override_invalid(hil_session, hil_base_url):
    r = hil_session.post(
        f"{hil_base_url}/api/v1/triac/override",
        json={"command": "not-a-valid-triac-cmd"},
        timeout=15,
    )
    assert r.status_code == 400, r.text
