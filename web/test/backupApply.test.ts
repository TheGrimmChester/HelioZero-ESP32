import { beforeEach, describe, expect, it, vi } from "vitest";
import { applyBackup, confirmRestoreBackupFromFile } from "../src/utils/backupApply";
import { buildBackup } from "../src/utils/backupFormat";
import {
  configForBackupImport,
  ensureRouterConfigPutPayload,
  READ_ONLY_CONFIG_KEYS,
} from "../src/api/configPut";
import type { ActionsConfigEnvelope, RouterConfig } from "../src/api/types";

const apiMock = vi.hoisted(() => ({
  putConfig: vi.fn(),
  putActionsConfig: vi.fn(),
  putTime: vi.fn(),
  putWifi: vi.fn(),
  getDevice: vi.fn(),
}));

vi.mock("../src/api/client", () => ({ api: apiMock }));
vi.mock("../src/components/Toast", () => ({ toast: vi.fn() }));
vi.mock("../src/components/Dialog", () => ({
  openDialog: vi.fn((opts: {
    actions: Array<{ onClick: () => void | Promise<void> }>;
  }) => {
    void Promise.resolve(opts.actions[1]?.onClick());
    return { close: () => {} };
  }),
}));

function sampleBackup() {
  const config = ensureRouterConfigPutPayload({ source: "UxIx2" } as RouterConfig);
  const actions: ActionsConfigEnvelope = {
    schema_version: 4,
    nb_actions: 1,
    actions: [
      {
        index: 0,
        title: "T",
        kind: "triac",
        repeat_sec: 0,
        tempo_sec: 0,
        regulation_mode: 1,
        periods: [],
      },
    ],
  };
  return buildBackup(
    config,
    actions,
    { tz: "UTC", ntp1: "a", ntp2: "b" },
    { ssid: "net", password: "" },
  );
}

describe("backupApply", () => {
  beforeEach(() => {
    vi.clearAllMocks();
    apiMock.putConfig.mockResolvedValue(undefined);
    apiMock.putActionsConfig.mockResolvedValue(undefined);
    apiMock.putTime.mockResolvedValue(undefined);
    apiMock.putWifi.mockResolvedValue(undefined);
    apiMock.getDevice.mockResolvedValue({
      router_name: "R",
      firmware_version: "1",
      probe_house_name: "H",
      probe_second_name: "S",
      temperature_label: "T",
    });
  });

  it("applyBackup succeeds on full chain", async () => {
    expect(await applyBackup(sampleBackup())).toBe("ok");
    expect(apiMock.putWifi).toHaveBeenCalled();
  });

  it("applyBackup sends all four exported sections to the API", async () => {
    const bindings = [
      {
        metric: "house_p",
        topic: "solar_router/state",
        format: "json" as const,
        path: "Pw",
      },
    ];
    const backup = buildBackup(
      ensureRouterConfigPutPayload({
        source: "Pmqtt",
        pmqtt_bindings: bindings,
      } as RouterConfig),
      {
        schema_version: 4,
        nb_actions: 1,
        actions: [
          {
            index: 0,
            title: "Tank",
            kind: "triac",
            repeat_sec: 0,
            tempo_sec: 0,
            regulation_mode: 1,
            periods: [{ mode: "on", hour_end: 24, power_min_w: 0, power_max_w: 0, temp_inf_c: 0, temp_sup_c: 99 }],
          },
        ],
      },
      { tz: "Europe/Paris", ntp1: "pool.ntp.org", ntp2: "time.google.com" },
      { ssid: "my-lan", password: "secret" },
    );
    expect(await applyBackup(backup)).toBe("ok");

    expect(apiMock.putConfig).toHaveBeenCalledTimes(1);
    const putCfg = apiMock.putConfig.mock.calls[0]![0] as RouterConfig;
    const expectedCfg = configForBackupImport(backup.config);
    expect(putCfg).toEqual(expectedCfg);
    expect(putCfg.pmqtt_bindings).toEqual(bindings);
    for (const k of READ_ONLY_CONFIG_KEYS) {
      expect(k in putCfg).toBe(false);
    }

    expect(apiMock.putActionsConfig).toHaveBeenCalledWith(backup.actions, expect.anything());
    expect(apiMock.putTime).toHaveBeenCalledWith(backup.time, expect.anything());
    expect(apiMock.putWifi).toHaveBeenCalledWith(
      { ssid: "my-lan", password: "secret", persist: true },
      expect.anything(),
    );
  });

  it("applyBackup returns config_failed", async () => {
    apiMock.putConfig.mockRejectedValue(new Error("fail"));
    expect(await applyBackup(sampleBackup())).toBe("config_failed");
  });

  it("applyBackup returns actions_failed on actions error", async () => {
    apiMock.putActionsConfig.mockRejectedValue(new Error("fail"));
    expect(await applyBackup(sampleBackup())).toBe("actions_failed");
  });

  it("applyBackup treats wifi network loss as ok", async () => {
    apiMock.putWifi.mockRejectedValue(new TypeError("Failed to fetch"));
    expect(await applyBackup(sampleBackup())).toBe("ok");
  });

  it("applyBackup treats wifi load failed message as ok", async () => {
    apiMock.putWifi.mockRejectedValue(new Error("Load failed"));
    expect(await applyBackup(sampleBackup())).toBe("ok");
  });

  it("applyBackup returns actions_failed on time error", async () => {
    apiMock.putTime.mockRejectedValue(new Error("fail"));
    expect(await applyBackup(sampleBackup())).toBe("actions_failed");
    expect(apiMock.getDevice).toHaveBeenCalled();
  });

  it("applyBackup returns actions_failed on wifi error", async () => {
    apiMock.putWifi.mockRejectedValue(new Error("server error"));
    expect(await applyBackup(sampleBackup())).toBe("actions_failed");
  });

  it("applyBackup refreshDevice ignores getDevice failure", async () => {
    apiMock.getDevice.mockRejectedValue(new Error("offline"));
    expect(await applyBackup(sampleBackup())).toBe("ok");
  });

  it("confirmRestoreBackupFromFile rejects invalid json", async () => {
    const file = new File(["not json"], "b.json");
    await confirmRestoreBackupFromFile(file);
    expect(apiMock.putConfig).not.toHaveBeenCalled();
  });

  it("confirmRestoreBackupFromFile rejects unreadable file", async () => {
    const file = {
      text: () => Promise.reject(new Error("read fail")),
    } as unknown as File;
    await confirmRestoreBackupFromFile(file);
    expect(apiMock.putConfig).not.toHaveBeenCalled();
  });

  it("confirmRestoreBackupFromFile rejects parse errors", async () => {
    const file = new File(['{"backupSchemaVersion":1}'], "b.json");
    await confirmRestoreBackupFromFile(file);
    expect(apiMock.putConfig).not.toHaveBeenCalled();
  });

});
