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
  putSystemBackup: vi.fn(),
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
    apiMock.putSystemBackup.mockResolvedValue(undefined);
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
    expect(apiMock.putSystemBackup).toHaveBeenCalled();
  });

  it("applyBackup sends full backup payload to PUT /system/backup", async () => {
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

    expect(apiMock.putSystemBackup).toHaveBeenCalledTimes(1);
    const payload = apiMock.putSystemBackup.mock.calls[0]![0] as ReturnType<typeof buildBackup> & {
      config: RouterConfig;
    };
    const expectedCfg = configForBackupImport(backup.config);
    expect(payload.config).toEqual(expectedCfg);
    expect(payload.config.pmqtt_bindings).toEqual(bindings);
    for (const k of READ_ONLY_CONFIG_KEYS) {
      expect(k in payload.config).toBe(false);
    }
    expect(payload.actions).toEqual(backup.actions);
    expect(payload.time).toEqual(backup.time);
    expect(payload.wifi).toEqual(backup.wifi);
  });

  it("applyBackup uses omitAuth on exempt paths", async () => {
    const prev = globalThis.location;
    vi.stubGlobal("location", { pathname: "/wifi" } as Location);
    await applyBackup(sampleBackup());
    const opts = apiMock.putSystemBackup.mock.calls[0]![1] as { omitAuth?: boolean };
    expect(opts?.omitAuth).toBe(true);
    vi.stubGlobal("location", prev);
  });

  it("applyBackup includes api block when present", async () => {
    const backup = sampleBackup();
    backup.api = {
      http_api_password: "lan-secret",
      access_tokens: [
        {
          id: 1,
          label: "HA",
          token: "a".repeat(64),
        },
      ],
    };
    expect(await applyBackup(backup)).toBe("ok");
    const payload = apiMock.putSystemBackup.mock.calls[0]![0] as typeof backup;
    expect(payload.api).toEqual(backup.api);
  });

  it("applyBackup returns config_failed", async () => {
    apiMock.putSystemBackup.mockRejectedValue(new Error("fail"));
    expect(await applyBackup(sampleBackup())).toBe("config_failed");
  });

  it("applyBackup treats network loss as ok", async () => {
    apiMock.putSystemBackup.mockRejectedValue(new TypeError("Failed to fetch"));
    expect(await applyBackup(sampleBackup())).toBe("ok");
  });

  it("applyBackup treats load failed message as ok", async () => {
    apiMock.putSystemBackup.mockRejectedValue(new Error("Load failed"));
    expect(await applyBackup(sampleBackup())).toBe("ok");
  });

  it("applyBackup refreshDevice ignores getDevice failure", async () => {
    apiMock.getDevice.mockRejectedValue(new Error("offline"));
    expect(await applyBackup(sampleBackup())).toBe("ok");
  });

  it("confirmRestoreBackupFromFile rejects invalid json", async () => {
    const file = new File(["not json"], "b.json");
    await confirmRestoreBackupFromFile(file);
    expect(apiMock.putSystemBackup).not.toHaveBeenCalled();
  });

  it("confirmRestoreBackupFromFile rejects unreadable file", async () => {
    const file = {
      text: () => Promise.reject(new Error("read fail")),
    } as unknown as File;
    await confirmRestoreBackupFromFile(file);
    expect(apiMock.putSystemBackup).not.toHaveBeenCalled();
  });

  it("confirmRestoreBackupFromFile rejects parse errors", async () => {
    const file = new File(['{"backupSchemaVersion":99}'], "b.json");
    await confirmRestoreBackupFromFile(file);
    expect(apiMock.putSystemBackup).not.toHaveBeenCalled();
  });

});
