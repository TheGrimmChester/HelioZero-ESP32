"""Shared helpers for HIL pytest modules."""

from __future__ import annotations

import json
import os
import time
from pathlib import Path
from typing import Any

import requests

FIXTURES = Path(__file__).resolve().parents[1] / "fixtures"


def get_with_retry(
    session: requests.Session,
    url: str,
    *,
    timeout: float = 20,
    attempts: int = 3,
    backoff_s: float = 2,
) -> requests.Response:
    """GET with retries on transient timeouts or connection errors."""
    last_exc: BaseException | None = None
    for attempt in range(attempts):
        try:
            r = session.get(url, timeout=timeout)
            if r.status_code == 200:
                return r
            if attempt + 1 < attempts and r.status_code >= 500:
                time.sleep(backoff_s)
                continue
            return r
        except (requests.Timeout, requests.ConnectionError) as exc:
            last_exc = exc
            if attempt + 1 < attempts:
                time.sleep(backoff_s)
    if last_exc is not None:
        raise last_exc
    raise RuntimeError("get_with_retry: no response")


def poll_wifi_scan(
    session: requests.Session,
    base_url: str,
    *,
    max_wait_s: float = 20,
    poll_s: float = 0.5,
    timeout: float = 20,
) -> dict[str, Any]:
    """Poll GET /api/v1/wifi/scan until scan completes (HTTP 200, scanning false)."""
    url = f"{base_url.rstrip('/')}/api/v1/wifi/scan"
    deadline = time.time() + max_wait_s
    while time.time() < deadline:
        r = session.get(url, timeout=timeout)
        if r.status_code not in (200, 202):
            r.raise_for_status()
        data = r.json()
        if r.status_code == 200 and not data.get("scanning", False):
            return data
        time.sleep(poll_s)
    raise TimeoutError(f"Wi-Fi scan did not complete within {max_wait_s}s")


def inject_payload(
    session: requests.Session,
    base_url: str,
    payload: dict[str, Any],
    *,
    timeout: float = 15,
) -> requests.Response:
    return session.post(f"{base_url.rstrip('/')}/api/v1/sources/test/inject", json=payload, timeout=timeout)


def inject_house(
    session: requests.Session,
    base_url: str,
    active_import_w: int,
    active_export_w: int,
    *,
    wall_decihours: int | None = None,
    temperature_c: float | None = None,
) -> requests.Response:
    payload: dict[str, Any] = {
        "house": {
            "active_import_w": active_import_w,
            "active_export_w": active_export_w,
        }
    }
    if wall_decihours is not None or temperature_c is not None:
        sim: dict[str, Any] = {}
        if wall_decihours is not None:
            sim["wall_decihours"] = wall_decihours
        if temperature_c is not None:
            sim["temperature_c"] = temperature_c
        payload["sim"] = sim
    return inject_payload(session, base_url, payload)


def read_state(session: requests.Session, base_url: str) -> dict[str, Any]:
    r = session.get(f"{base_url.rstrip('/')}/api/v1/state", timeout=15)
    r.raise_for_status()
    return r.json()


def triac_open_percent(session: requests.Session, base_url: str) -> float:
    return float(read_state(session, base_url)["triac_open_percent"])


def wait_triac_stable(
    session: requests.Session,
    base_url: str,
    *,
    timeout_s: float = 3.0,
    poll_s: float = 0.25,
    tolerance: float = 1.0,
) -> float:
    deadline = time.time() + timeout_s
    last = triac_open_percent(session, base_url)
    while time.time() < deadline:
        time.sleep(poll_s)
        cur = triac_open_percent(session, base_url)
        if abs(cur - last) <= tolerance:
            return cur
        last = cur
    return last


def require_inject_api(session: requests.Session, base_url: str) -> None:
    r = inject_house(session, base_url, 0, 0)
    if r.status_code == 404:
        import pytest

        pytest.skip("HIL inject route not available (flash pio run -e hil)")


def regulation_required() -> bool:
    from hil_env import hil_env

    return hil_env("HELIO_ZERO_HIL_REQUIRE_REGULATION", default="").strip() in (
        "1",
        "true",
        "yes",
    )


def skip_or_fail_not_regulating(pct_import: float, pct_export: float) -> None:
    import pytest

    if pct_import == 0.0 and pct_export == 0.0:
        msg = "Triac stayed at 0% — enable action 0 with auto schedule on HIL device"
        if regulation_required():
            pytest.fail(msg)
        pytest.skip(msg)


def load_json_fixture(*parts: str) -> Any:
    return json.loads((FIXTURES.joinpath(*parts)).read_text(encoding="utf-8"))
