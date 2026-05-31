import type { ActionsConfigEnvelope, RouterConfig } from "../api/types";
import { PERSISTED_CONFIG_KEYS, pickPersistedConfig } from "../api/configPut";
import { ensureNormalised } from "../routes/actions/model";
export { sanitizeConfigForImport } from "../api/configPut";

/** Match Actions page when device GET returns `actions: []` (NbActions == 0). */
export function normalizeActionsForBackup(
  env: ActionsConfigEnvelope,
): ActionsConfigEnvelope {
  const actions = ensureNormalised(env.actions ?? []);
  return {
    schema_version: env.schema_version,
    nb_actions: actions.length,
    actions,
  };
}

/** Client-side backup file format (not the firmware REST envelope). */
export const BACKUP_SCHEMA_VERSION = 3 as const;
/** Legacy export versions — import is upgraded to v3 in parseBackupJson. */
export const BACKUP_SCHEMA_VERSION_LEGACY = 1 as const;
export const BACKUP_SCHEMA_VERSION_V2 = 2 as const;
/** Matches firmware kMaxRoutingActions / actions UI cap. */
export const BACKUP_MAX_ACTIONS = 20;

const BACKUP_TOP_LEVEL_KEYS = [
  "backupSchemaVersion",
  "exportedAt",
  "config",
  "actions",
  "time",
  "wifi",
  "api",
] as const;

export interface HelioZeroBackupApiAccessToken {
  id: number;
  label: string;
  token: string;
}

export interface HelioZeroBackupApi {
  http_api_password?: string;
  access_tokens?: HelioZeroBackupApiAccessToken[];
}

export interface HelioZeroBackupTime {
  tz: string;
  ntp1: string;
  ntp2: string;
}

export interface HelioZeroBackupWifi {
  ssid: string;
  password: string;
}

export interface HelioZeroBackup {
  backupSchemaVersion: typeof BACKUP_SCHEMA_VERSION;
  exportedAt: string;
  config: RouterConfig;
  actions: ActionsConfigEnvelope;
  time: HelioZeroBackupTime;
  wifi: HelioZeroBackupWifi;
  api?: HelioZeroBackupApi;
}

const TOKEN_HEX64 = /^[0-9a-f]{64}$/;

function parseBackupApi(
  raw: unknown,
): { ok: true; api: HelioZeroBackupApi } | { ok: false; errorKey: string } {
  if (!isPlainObject(raw)) {
    return { ok: false, errorKey: "badApi" };
  }
  const api: HelioZeroBackupApi = {};
  if ("http_api_password" in raw) {
    if (typeof raw["http_api_password"] !== "string") {
      return { ok: false, errorKey: "badApi" };
    }
    api.http_api_password = raw["http_api_password"];
  }
  if ("access_tokens" in raw) {
    const arr = raw["access_tokens"];
    if (!Array.isArray(arr)) {
      return { ok: false, errorKey: "badApiTokens" };
    }
    if (arr.length > 4) {
      return { ok: false, errorKey: "badApiTokens" };
    }
    const tokens: HelioZeroBackupApiAccessToken[] = [];
    for (const item of arr) {
      if (!isPlainObject(item)) {
        return { ok: false, errorKey: "badApiTokens" };
      }
      if (typeof item["id"] !== "number" || typeof item["token"] !== "string") {
        return { ok: false, errorKey: "badApiTokens" };
      }
      if (!TOKEN_HEX64.test(item["token"])) {
        return { ok: false, errorKey: "badApiTokens" };
      }
      tokens.push({
        id: item["id"],
        label: typeof item["label"] === "string" ? item["label"] : "",
        token: item["token"],
      });
    }
    api.access_tokens = tokens;
  }
  return { ok: true, api };
}

function isPlainObject(v: unknown): v is Record<string, unknown> {
  return typeof v === "object" && v !== null && !Array.isArray(v);
}

function isNonEmptyString(v: unknown): v is string {
  return typeof v === "string";
}

export function buildBackup(
  config: RouterConfig,
  actions: ActionsConfigEnvelope,
  time: HelioZeroBackupTime,
  wifi: HelioZeroBackupWifi,
): HelioZeroBackup {
  const actionsNorm = normalizeActionsForBackup(actions);
  return {
    backupSchemaVersion: BACKUP_SCHEMA_VERSION,
    exportedAt: new Date().toISOString(),
    config: pickPersistedConfig(config),
    actions: {
      schema_version: actionsNorm.schema_version,
      nb_actions: actionsNorm.nb_actions,
      actions: actionsNorm.actions.map((a) => ({
        ...a,
        periods: a.periods.map((p) => ({ ...p })),
      })),
    },
    time: { ...time },
    wifi: { ...wifi },
  };
}

export function parseBackupJson(
  raw: string,
): { ok: true; backup: HelioZeroBackup } | { ok: false; errorKey: string } {
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw) as unknown;
  } catch {
    return { ok: false, errorKey: "invalidJson" };
  }
  if (!isPlainObject(parsed)) {
    return { ok: false, errorKey: "notObject" };
  }
  const ver = parsed["backupSchemaVersion"];
  if (
    ver !== BACKUP_SCHEMA_VERSION &&
    ver !== BACKUP_SCHEMA_VERSION_LEGACY &&
    ver !== BACKUP_SCHEMA_VERSION_V2
  ) {
    return { ok: false, errorKey: "badSchemaVersion" };
  }
  for (const k of Object.keys(parsed)) {
    if (!(BACKUP_TOP_LEVEL_KEYS as readonly string[]).includes(k)) {
      return { ok: false, errorKey: "unknownTopLevelKey" };
    }
  }
  if (!isNonEmptyString(parsed["exportedAt"])) {
    return { ok: false, errorKey: "missingExportedAt" };
  }
  const cfgRaw = parsed["config"];
  if (!isPlainObject(cfgRaw)) {
    return { ok: false, errorKey: "missingConfig" };
  }
  const cfg = cfgRaw;
  for (const k of PERSISTED_CONFIG_KEYS) {
    if (!(k in cfg)) {
      return { ok: false, errorKey: "incompleteConfig" };
    }
  }
  if (cfg["source"] === "Pmqtt") {
    if (!("pmqtt_bindings" in cfg)) {
      return { ok: false, errorKey: "missingPmqttBindings" };
    }
    if (!Array.isArray(cfg["pmqtt_bindings"])) {
      return { ok: false, errorKey: "badPmqttBindings" };
    }
  }
  const act = parsed["actions"];
  if (!isPlainObject(act)) {
    return { ok: false, errorKey: "missingActions" };
  }
  let arr = act["actions"];
  if (!Array.isArray(arr)) {
    return { ok: false, errorKey: "missingActions" };
  }
  let actionsList: unknown[] = arr;
  if (actionsList.length === 0) {
    actionsList = ensureNormalised([]) as unknown[];
  }
  if (actionsList.length > BACKUP_MAX_ACTIONS) {
    return { ok: false, errorKey: "actionsTooMany" };
  }
  if (typeof act["schema_version"] !== "number") {
    return { ok: false, errorKey: "actionsBadEnvelope" };
  }
  const timeObj = parsed["time"];
  if (!isPlainObject(timeObj)) {
    return { ok: false, errorKey: "missingTime" };
  }
  if (
    !isNonEmptyString(timeObj["tz"]) ||
    typeof timeObj["ntp1"] !== "string" ||
    typeof timeObj["ntp2"] !== "string"
  ) {
    return { ok: false, errorKey: "badTime" };
  }
  const wifiObj = parsed["wifi"];
  if (!isPlainObject(wifiObj)) {
    return { ok: false, errorKey: "missingWifi" };
  }
  if (!isNonEmptyString(wifiObj["ssid"]) || typeof wifiObj["password"] !== "string") {
    return { ok: false, errorKey: "badWifi" };
  }
  let apiBlock: HelioZeroBackupApi | undefined;
  if ("api" in parsed) {
    const apiParsed = parseBackupApi(parsed["api"]);
    if (!apiParsed.ok) {
      return { ok: false, errorKey: apiParsed.errorKey };
    }
    const hasPassword =
      apiParsed.api.http_api_password !== undefined &&
      apiParsed.api.http_api_password.length > 0;
    const hasTokens =
      apiParsed.api.access_tokens !== undefined && apiParsed.api.access_tokens.length > 0;
    if (hasPassword || hasTokens) {
      apiBlock = apiParsed.api;
    }
  }

  const backup: HelioZeroBackup = {
    backupSchemaVersion: BACKUP_SCHEMA_VERSION,
    exportedAt: parsed["exportedAt"],
    config: cfg as unknown as RouterConfig,
    actions: {
      schema_version: act["schema_version"] as number,
      nb_actions: typeof act["nb_actions"] === "number" ? act["nb_actions"] : actionsList.length,
      actions: actionsList as unknown as ActionsConfigEnvelope["actions"],
    },
    time: {
      tz: timeObj["tz"],
      ntp1: timeObj["ntp1"],
      ntp2: timeObj["ntp2"],
    },
    wifi: {
      ssid: wifiObj["ssid"],
      password: wifiObj["password"],
    },
    ...(apiBlock !== undefined ? { api: apiBlock } : {}),
  };
  return { ok: true, backup };
}

export function downloadJsonFile(filename: string, data: unknown): void {
  const blob = new Blob([JSON.stringify(data, null, 2)], {
    type: "application/json",
  });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = filename;
  a.rel = "noopener";
  document.body.append(a);
  a.click();
  a.remove();
  URL.revokeObjectURL(url);
}

export function backupDownloadFilename(): string {
  const d = new Date();
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, "0");
  const day = String(d.getDate()).padStart(2, "0");
  return `helio-zero-${y}-${m}-${day}.json`;
}
