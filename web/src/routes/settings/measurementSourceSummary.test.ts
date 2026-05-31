import { describe, expect, it } from "vitest";
import { sourceKeyForSummary } from "./measurementSourceSummary";

describe("sourceKeyForSummary", () => {
  it("uses configured label when present", () => {
    expect(sourceKeyForSummary("HelioPeer", "JsyMk194")).toBe("heliopeer");
  });

  it("falls back to cfg.source when configured empty", () => {
    expect(sourceKeyForSummary("", "HelioPeer")).toBe("heliopeer");
    expect(sourceKeyForSummary("", undefined)).toBe("notdef");
  });
});
