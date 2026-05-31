import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { en } from "../src/i18n/locales/en";
import { fr } from "../src/i18n/locales/fr";
import { deviceInfo } from "../src/state/store";
import { firmwareUpdate } from "../src/state/firmwareUpdate";

const runCheckMock = vi.hoisted(() => vi.fn());

vi.mock("../src/components/Toast", () => ({ toast: vi.fn() }));
vi.mock("../src/firmware/githubDailyCheck", async (importOriginal) => {
  const actual = await importOriginal<typeof import("../src/firmware/githubDailyCheck")>();
  return {
    ...actual,
    runFirmwareUpdateCheck: runCheckMock,
  };
});

import { toast } from "../src/components/Toast";
import { buildFirmwareUpdateCard } from "../src/routes/firmwareUpdateCard";

describe("firmwareUpdateCard", () => {
  beforeEach(() => {
    firmwareUpdate.reset();
    deviceInfo.set({ firmware_version: "0.1.0" } as never);
    runCheckMock.mockReset();
    vi.mocked(toast).mockReset();
    document.body.innerHTML = "";
  });

  afterEach(() => {
    document.body.innerHTML = "";
  });

  it("renders channel picker and triggers forced check", async () => {
    runCheckMock.mockResolvedValue({
      ok: true,
      result: {
        release: { tag_name: "v0.2.0" },
        compare: -1,
      },
    });
    firmwareUpdate.set({
      available: true,
      releaseTag: "v0.2.0",
      compare: -1,
      checking: false,
      error: "",
    });

    const { section } = buildFirmwareUpdateCard(en);
    document.body.append(section);

    expect(section.querySelector("#firmware_fw_stable")).not.toBeNull();
    expect(section.querySelector("#firmware_fw_prerelease")).not.toBeNull();
    expect(section.textContent).toContain(en.firmware.updatesTitle);

    const btn = section.querySelector("button") as HTMLButtonElement;
    expect(btn.textContent).toContain(en.firmware.forceCheckBtn);
    btn.click();
    await Promise.resolve();
    expect(runCheckMock).toHaveBeenCalledWith({ force: true });
    expect(toast).toHaveBeenCalled();
  });

  it("uses French force check label", () => {
    const { section } = buildFirmwareUpdateCard(fr);
    const btn = section.querySelector("button") as HTMLButtonElement;
    expect(btn.textContent).toContain(fr.firmware.forceCheckBtn);
  });

  it("switches channel and re-hydrates from storage", () => {
    const { section } = buildFirmwareUpdateCard(en);
    document.body.append(section);

    const prerelease = section.querySelector(
      "#firmware_fw_prerelease",
    ) as HTMLInputElement;
    prerelease.checked = true;
    prerelease.dispatchEvent(new Event("change", { bubbles: true }));

    expect(prerelease.checked).toBe(true);
  });

  it("shows error toast when check fails", async () => {
    runCheckMock.mockResolvedValue({
      ok: false,
      error: new Error("network"),
    });

    const { section } = buildFirmwareUpdateCard(en);
    document.body.append(section);
    const btn = section.querySelector("button") as HTMLButtonElement;
    btn.click();
    await Promise.resolve();

    expect(toast).toHaveBeenCalledWith(expect.any(String), "error");
  });

  it("disables controls while checking", () => {
    const { section, refresh } = buildFirmwareUpdateCard(en);
    document.body.append(section);
    firmwareUpdate.set({ checking: true });
    refresh();

    const btn = section.querySelector("button") as HTMLButtonElement;
    expect(btn.hasAttribute("disabled")).toBe(true);
  });
});
