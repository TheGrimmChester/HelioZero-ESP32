from __future__ import annotations

import os

import pytest
import requests

from hil_env import hil_env

OPENAPI_PATHS = [
    "/api/v1/measurements",
    "/api/v1/system",
    "/api/v1/device",
    "/api/v1/state",
    "/api/v1/health",
    "/api/v1/sources",
    "/api/v1/config",
    "/api/v1/actions",
    "/api/v1/history/power",
    "/api/v1/gpio",
    "/api/v1/openapi.json",
]


@pytest.fixture(scope="session")
def hil_base_url() -> str:
    url = hil_env("HELIO_ZERO_HIL_URL", default="").rstrip("/")
    if not url:
        pytest.skip("HELIO_ZERO_HIL_URL not set")
    return url


@pytest.fixture(scope="session")
def hil_auth():
    password = hil_env("HELIO_ZERO_HIL_PASSWORD", default="")
    if not password:
        return None
    user = hil_env("HELIO_ZERO_HIL_USER", default="admin")
    return (user, password)


@pytest.fixture(scope="session")
def hil_session(hil_base_url: str, hil_auth):
    s = requests.Session()
    s.auth = hil_auth
    r = s.get(f"{hil_base_url}/api/v1/health", timeout=10)
    r.raise_for_status()
    return s


@pytest.fixture(scope="session")
def openapi_paths(hil_session, hil_base_url: str) -> list[str]:
    r = hil_session.get(f"{hil_base_url}/api/v1/openapi.json", timeout=15)
    r.raise_for_status()
    doc = r.json()
    return sorted(doc.get("paths", {}).keys())
