from __future__ import annotations

import time

from hil_helpers import (
    inject_house,
    load_json_fixture,
    require_inject_api,
    skip_or_fail_not_regulating,
    triac_open_percent,
    wait_triac_stable,
)


def test_profile_ramp_export_increases_triac(hil_session, hil_base_url):
    require_inject_api(hil_session, hil_base_url)
    profile = load_json_fixture("regulation", "profile_ramp.json")
    prev = triac_open_percent(hil_session, hil_base_url)
    for step in profile["steps"]:
        inject_house(
            hil_session,
            hil_base_url,
            step["active_import_w"],
            step["active_export_w"],
        )
        time.sleep(float(step.get("dwell_s", 1.0)))
        prev = wait_triac_stable(hil_session, hil_base_url, timeout_s=2.0)
    pct_import = triac_open_percent(hil_session, hil_base_url)
    inject_house(hil_session, hil_base_url, 4000, 0)
    time.sleep(1.0)
    pct_import = wait_triac_stable(hil_session, hil_base_url)
    inject_house(hil_session, hil_base_url, 80, 3500)
    time.sleep(1.0)
    pct_export = wait_triac_stable(hil_session, hil_base_url)
    skip_or_fail_not_regulating(pct_import, pct_export)
    assert pct_export > pct_import
