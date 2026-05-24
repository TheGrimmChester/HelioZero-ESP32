#!/usr/bin/env python3
"""Poll GET /api/v1/health until the HIL device responds."""

from __future__ import annotations

import sys
import time

import requests

from hil_env import hil_env

TIMEOUT_S = int(hil_env("HELIO_ZERO_HIL_WAIT_S", default="120"))
INTERVAL_S = 2


def main() -> int:
    base = hil_env("HELIO_ZERO_HIL_URL", default="").rstrip("/")
    if not base:
        print("ERROR: HELIO_ZERO_HIL_URL is not set", file=sys.stderr)
        return 2
    auth = None
    user = hil_env("HELIO_ZERO_HIL_USER", default="admin")
    password = hil_env("HELIO_ZERO_HIL_PASSWORD", default="")
    if password:
        auth = (user, password)
    deadline = time.time() + TIMEOUT_S
    url = f"{base}/api/v1/health"
    while time.time() < deadline:
        try:
            r = requests.get(url, auth=auth, timeout=5)
            if r.status_code == 200:
                print(f"OK: device ready at {base}")
                return 0
        except requests.RequestException:
            pass
        time.sleep(INTERVAL_S)
    print(f"ERROR: device not ready at {url} within {TIMEOUT_S}s", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
