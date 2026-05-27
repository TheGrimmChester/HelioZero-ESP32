import { describe, expect, it } from "vitest";
import {
  apparentHasSignal,
  buildPowerTimeAxisHours,
  buildPowerTimeAxisSeconds,
  formatPowerAxisHours,
  formatPowerAxisSeconds,
  maxAbs,
  padSeries,
  powerHasSignal,
} from "../src/utils/historyPower";
import {
  energyDayIsoDates as energyDates,
  parseDeviceDateTime,
  toIsoDateLocal,
} from "../src/utils/historyEnergy";

describe("historyPower", () => {
  it("powerHasSignal detects non-flat series", () => {
    expect(powerHasSignal([0, 0], [0])).toBe(false);
    expect(powerHasSignal([0, 42], [])).toBe(true);
    expect(powerHasSignal([], [5])).toBe(true);
  });

  it("buildPowerTimeAxisSeconds ends at 0", () => {
    const xs = buildPowerTimeAxisSeconds(3, 2);
    expect(xs[0]).toBe(-4);
    expect(xs[1]).toBe(-2);
    expect(xs[2]).toBe(0);
  });

  it("padSeries extends with zeros", () => {
    expect(padSeries([10], 3)).toEqual([10, 0, 0]);
  });

  it("formatPowerAxisSeconds", () => {
    expect(formatPowerAxisSeconds(-125)).toBe("2:05");
    expect(formatPowerAxisSeconds(-60)).toBe("1 min");
    expect(formatPowerAxisSeconds(-30)).toBe("30s");
    expect(formatPowerAxisSeconds(0)).toBe("0s");
  });

  it("maxAbs ignores non-finite values", () => {
    expect(maxAbs([NaN, -3, Infinity])).toBe(3);
    expect(maxAbs([])).toBe(0);
  });

  it("padSeries truncates when longer than target", () => {
    expect(padSeries([1, 2, 3, 4], 2)).toEqual([1, 2]);
  });

  it("apparentHasSignal", () => {
    expect(apparentHasSignal([], [])).toBe(false);
    expect(apparentHasSignal([500], [])).toBe(true);
  });

  it("buildPowerTimeAxisSeconds uses default period when invalid", () => {
    expect(buildPowerTimeAxisSeconds(2, 0)).toEqual([-2, 0]);
  });

  it("buildPowerTimeAxisHours and formatPowerAxisHours", () => {
    const xs = buildPowerTimeAxisHours(2, 3600);
    expect(xs[0]).toBeCloseTo(-1, 5);
    expect(xs[1]).toBeCloseTo(0, 5);
    expect(formatPowerAxisHours(0)).toBe("0");
    expect(formatPowerAxisHours(-1)).toBe("1 h");
    expect(formatPowerAxisHours(-1.25)).toBe("1h15");
  });
});

describe("historyEnergy", () => {
  it("parseDeviceDateTime", () => {
    const d = parseDeviceDateTime("22/05/2026 14:30:00");
    expect(d).not.toBeNull();
    expect(toIsoDateLocal(d!)).toBe("2026-05-22");
  });

  it("energyDayIsoDates from reference", () => {
    expect(energyDates(3, "2026-05-22")).toEqual([
      "2026-05-20",
      "2026-05-21",
      "2026-05-22",
    ]);
  });
});
