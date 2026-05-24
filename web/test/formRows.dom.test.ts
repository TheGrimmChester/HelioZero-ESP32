import { describe, expect, it } from "vitest";
import {
  numberRow,
  passwordRow,
  textRow,
  validateIp,
} from "../src/routes/settings/formRows";

describe("formRows", () => {
  it("validateIp toggles error state", () => {
    const { ref } = textRow("ip", "IP", "192.168.1.1", "");
    expect(validateIp(ref)).toBe(true);
    ref.write("999.1.1.1");
    expect(validateIp(ref)).toBe(false);
  });

  it("passwordRow and numberRow build fields", () => {
    const pw = passwordRow("pw", "Password", "secret", "hint");
    expect(pw.ref.read()).toBe("secret");
    const num = numberRow("n", "Num", 42, "hint", () => {});
    expect(num.ref.read()).toBe("42");
  });

  it("textRow with help uses FieldHelp label", () => {
    const { el } = textRow("t", "Label", "val", "hint", {
      helpScope: "settings",
      helpKey: "router_name",
    });
    expect(el.querySelector(".field-help")).not.toBeNull();
  });
});
