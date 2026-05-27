import type { ActionConfig, ActionPeriod, PeriodMode } from "../../api/types";
import { getStrings } from "../../i18n";
import { MODE_DECOUPE_ONOFF, MODE_INACTIF } from "./regulationMode";

export const SCHEMA_VERSION = 2;
export const MAX_PERIODS = 8;
export const MAX_ACTIONS = 20;

export function ensureNormalised(arr: ActionConfig[]): ActionConfig[] {
  if (!arr || !arr.length) {
    const T = getStrings();
    return [
      {
        index: 0,
        regulation_mode: MODE_DECOUPE_ONOFF,
        kind: "triac",
        title: T.actions.triacName,
        triac_sensitivity: 50,
        repeat_sec: 0,
        tempo_sec: 0,
        periods: [
          {
            mode: "power",
            hour_end: 2400,
            power_min_w: 0,
            power_max_w: 100,
            temp_inf_c: 150,
            temp_sup_c: 150,
          },
        ],
      },
    ];
  }
  return arr.map((a, idx) => ({
    ...a,
    index: idx,
    triac_sensitivity:
      a.kind === "triac" ? a.triac_sensitivity ?? a.port ?? 50 : a.triac_sensitivity,
    port: a.kind === "triac" ? a.triac_sensitivity ?? a.port ?? 50 : a.port ?? 80,
    periods: (a.periods || []).map((p) => ({ ...p })),
  }));
}

export function blankHttpAction(idx: number): ActionConfig {
  return {
    index: idx,
    regulation_mode: MODE_INACTIF,
    kind: "remote_http",
    title: getStrings().actions.newActionDefault,
    host: "192.168.1.10",
    port: 80,
    path_on: "",
    path_off: "",
    repeat_sec: 60,
    tempo_sec: 0,
    periods: [
      {
        mode: "off" as PeriodMode,
        hour_end: 2400,
        power_min_w: 0,
        power_max_w: 0,
        temp_inf_c: 150,
        temp_sup_c: 150,
      } satisfies ActionPeriod,
    ],
  };
}

export function normaliseForApi(a: ActionConfig, idx: number): ActionConfig {
  const out: ActionConfig = {
    index: idx,
    regulation_mode: a.regulation_mode ?? MODE_INACTIF,
    kind: idx === 0 ? "triac" : a.host === "localhost" ? "local_gpio" : "remote_http",
    title: a.title,
    repeat_sec: a.repeat_sec || 0,
    tempo_sec: a.tempo_sec || 0,
    periods: a.periods.map((p) => ({ ...p })),
  };
  if (idx === 0) {
    out.triac_sensitivity = a.triac_sensitivity ?? a.port ?? 50;
  } else {
    out.host = a.host || "";
    out.port = a.port || 80;
    out.path_on = a.path_on || "";
    out.path_off = a.path_off || "";
  }
  return out;
}

/** Firmware meters daily routed energy for triac (index 0) via CH2 export only. */
export function actionSupportsDailyCapWh(index: number): boolean {
  return index === 0;
}

export function normalizeDailyCapWh(fromConfig?: number[]): [number, number, number] {
  const a = fromConfig ?? [];
  return [a[0] ?? 0, a[1] ?? 0, a[2] ?? 0];
}
