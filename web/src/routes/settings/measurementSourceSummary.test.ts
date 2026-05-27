import { describe, expect, it } from "vitest";
import { sourceKeyForSummary } from "./measurementSourceSummary";

describe("sourceKeyForSummary", () => {
  it("uses configured label when present", () => {
    expect(sourceKeyForSummary("Ext", "UxIx2")).toBe("ext");
  });

  it("falls back to cfg.source when configured empty", () => {
    expect(sourceKeyForSummary("", "Ext")).toBe("ext");
    expect(sourceKeyForSummary("", undefined)).toBe("notdef");
  });
});
