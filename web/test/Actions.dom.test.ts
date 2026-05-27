import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { ensureNormalised } from "../src/routes/actions/model";
import { getStrings } from "../src/i18n";

const apiMock = vi.hoisted(() => ({
  getActionsLive: vi.fn(),
  getActionsConfig: vi.fn(),
  getConfig: vi.fn(),
  putActionsConfig: vi.fn(),
  patchConfig: vi.fn(),
}));

vi.mock("../src/api/client", () => ({ api: apiMock }));
vi.mock("../src/components/Toast", () => ({ toast: vi.fn() }));

import { mountActions } from "../src/routes/Actions";
import { toast } from "../src/components/Toast";

function mockApiDefaults() {
  const actions = ensureNormalised([]);
  apiMock.getActionsLive.mockResolvedValue({ temperature_c: 45 });
  apiMock.getActionsConfig.mockResolvedValue({
    schema_version: 2,
    nb_actions: actions.length,
    actions,
  });
  apiMock.getConfig.mockResolvedValue({
    config: { action_daily_cap_wh: [0, 0, 0] },
  });
  apiMock.putActionsConfig.mockResolvedValue(undefined);
  apiMock.patchConfig.mockResolvedValue(undefined);
}

describe("Actions route save", () => {
  beforeEach(() => {
    vi.useFakeTimers();
    vi.clearAllMocks();
    mockApiDefaults();
    document.body.innerHTML = "";
  });

  afterEach(() => {
    vi.useRealTimers();
    document.body.innerHTML = "";
  });

  it("autosaves after debounce when title changes", async () => {
    const outlet = document.createElement("main");
    document.body.append(outlet);
    const ctrl = new AbortController();
    const cleanup = await mountActions({ outlet, signal: ctrl.signal, path: "/actions" });

    const title = outlet.querySelector('input[type="text"]') as HTMLInputElement;
    title.value = "Updated triac";
    title.dispatchEvent(new Event("input", { bubbles: true }));

    expect(outlet.querySelector(".card__save-status--dirty")).not.toBeNull();

    await vi.advanceTimersByTimeAsync(700);
    expect(apiMock.putActionsConfig).toHaveBeenCalledTimes(1);
    expect(apiMock.patchConfig).toHaveBeenCalledWith(
      { action_daily_cap_wh: [0, 0, 0] },
      expect.objectContaining({ retry: 1 }),
    );
    expect(vi.mocked(toast)).toHaveBeenCalledWith(getStrings().saved, "success");

    cleanup();
    ctrl.abort();
  });

  it("header save flushes immediately before debounce completes", async () => {
    const outlet = document.createElement("main");
    document.body.append(outlet);
    const ctrl = new AbortController();
    const cleanup = await mountActions({ outlet, signal: ctrl.signal, path: "/actions" });

    const title = outlet.querySelector('input[type="text"]') as HTMLInputElement;
    title.value = "Quick save";
    title.dispatchEvent(new Event("input", { bubbles: true }));

    const saveBtn = Array.from(outlet.querySelectorAll("button")).find((b) =>
      b.textContent?.includes(getStrings().save),
    ) as HTMLButtonElement;
    expect(saveBtn).toBeDefined();
    expect(saveBtn.disabled).toBe(false);
    saveBtn.click();

    await vi.advanceTimersByTimeAsync(0);
    expect(apiMock.putActionsConfig).toHaveBeenCalledTimes(1);

    cleanup();
    ctrl.abort();
  });
});
