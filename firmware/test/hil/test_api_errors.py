from __future__ import annotations

import json
import os
from pathlib import Path

import pytest

GOLDEN_DIR = Path(__file__).resolve().parents[1] / "golden"


@pytest.mark.skipif(not os.environ.get("HELIO_ZERO_HIL_PASSWORD"), reason="auth not configured")
def test_unauthorized_returns_error_envelope(hil_base_url):
    import requests

    r = requests.get(
        f"{hil_base_url}/api/v1/device",
        auth=("admin", "wrong-password"),
        timeout=15,
    )
    assert r.status_code == 401
    payload = r.json()
    spec = json.loads((GOLDEN_DIR / "required_error_keys.json").read_text(encoding="utf-8"))
    for key in spec["required_keys"]:
        assert key in payload
