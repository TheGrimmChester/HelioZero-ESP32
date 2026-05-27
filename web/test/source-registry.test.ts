import { describe, expect, it } from "vitest";
import {
  SOURCE_REGISTRY,
  pinoutSectionUrl,
  registryEntry,
  type SourceWireId,
} from "../src/routes/settings/sourceRegistry";

describe("source registry", () => {
  it("lists 11 configured sources", () => {
    expect(SOURCE_REGISTRY).toHaveLength(11);
    expect(registryEntry("Linky")?.pinoutAnchor).toBe("source_linky");
    expect(registryEntry("Ext")?.fields).toContain("ext_peer_path");
  });

  it("every entry has id, pinout anchor, and fields array", () => {
    for (const e of SOURCE_REGISTRY) {
      expect(e.id).toBeTruthy();
      expect(e.pinoutAnchor).toBeTruthy();
      expect(Array.isArray(e.fields)).toBe(true);
      expect(registryEntry(e.id)?.id).toBe(e.id);
    }
  });

  it("pinoutSectionUrl points at hardware pinout anchor", () => {
    const url = pinoutSectionUrl("source_linky");
    expect(url).toBe(
      "https://heliozero.clouded.fr/en/hardware-pinout/#source_linky",
    );
  });

  it("maps expected fields per source", () => {
    const expected: Partial<Record<SourceWireId, string[]>> = {
      UxI: ["calib_u", "calib_i"],
      Pmqtt: ["pmqtt_topic", "pmqtt_schema"],
      Enphase: ["enphase_user", "enphase_password", "enphase_serial"],
      ShellyPro: ["ext_peer_ip", "meter_channel"],
    };
    for (const [id, fields] of Object.entries(expected)) {
      expect(registryEntry(id as SourceWireId)?.fields).toEqual(fields);
    }
  });
});
