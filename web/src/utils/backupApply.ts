import { api } from "../api/client";
import { isAuthExemptPath } from "../auth/httpAuthGate";
import { deviceInfo } from "../state/store";
import { toast } from "../components/Toast";
import { openDialog } from "../components/Dialog";
import { getStrings } from "../i18n";
import { h } from "./dom";
import { configForBackupImport } from "../api/configPut";
import type { HelioZeroBackup } from "./backupFormat";
import { parseBackupJson } from "./backupFormat";

export async function applyBackup(
  backup: HelioZeroBackup,
  signal?: AbortSignal,
): Promise<"ok" | "config_failed" | "actions_failed"> {
  const T = getStrings();
  const omitAuth =
    typeof location !== "undefined" && isAuthExemptPath(location.pathname || "/");
  const opts = { signal, retry: 0 as const, omitAuth };

  try {
    await api.putConfig(configForBackupImport(backup.config), opts);
  } catch (e) {
    console.error(e);
    toast(T.saveError, "error");
    return "config_failed";
  }

  try {
    await api.putActionsConfig(backup.actions, opts);
  } catch (e) {
    console.error(e);
    toast(T.backup.importPartialWarn, "error", 8000);
    await refreshDevice(signal);
    return "actions_failed";
  }

  try {
    await api.putTime(backup.time, opts);
  } catch (e) {
    console.error(e);
    toast(T.backup.importPartialWarn, "error", 8000);
    await refreshDevice(signal);
    return "actions_failed";
  }

  try {
    await api.putWifi(
      {
        ssid: backup.wifi.ssid,
        password: backup.wifi.password,
        persist: true,
      },
      opts,
    );
  } catch (e) {
    const networkLost =
      e instanceof TypeError ||
      (e instanceof Error && /failed to fetch|networkerror|load failed/i.test(e.message));
    if (networkLost) {
      toast(T.wifi.saveApLikelyOk, "success", 8000);
      return "ok";
    }
    console.error(e);
    toast(T.backup.importPartialWarn, "error", 8000);
    await refreshDevice(signal);
    return "actions_failed";
  }

  toast(T.backup.importSuccess, "success");
  toast(T.backup.rebootHint, "info", 6000);
  await refreshDevice(signal);
  return "ok";
}

export async function confirmRestoreBackupFromFile(
  file: File,
  signal?: AbortSignal,
): Promise<void> {
  const T = getStrings();
  let text: string;
  try {
    text = await file.text();
  } catch {
    toast(T.backup.parseErrors.invalidJson, "error");
    return;
  }
  const parsed = parseBackupJson(text);
  if (!parsed.ok) {
    const msg =
      T.backup.parseErrors[parsed.errorKey as keyof typeof T.backup.parseErrors] ||
      T.unknown;
    toast(msg, "error");
    return;
  }
  const backup = parsed.backup;
  openDialog({
    title: T.backup.importConfirmTitle,
    body: h("p", {}, T.backup.importConfirmBody),
    actions: [
      { label: T.cancel, kind: "ghost", onClick: () => {} },
      {
        label: T.backup.importApply,
        kind: "danger",
        onClick: async () => {
          await applyBackup(backup, signal);
        },
      },
    ],
  });
}

async function refreshDevice(signal?: AbortSignal): Promise<void> {
  try {
    const d = await api.getDevice({ signal, retry: 1 });
    deviceInfo.set({
      router_name: d.router_name,
      firmware_version: d.firmware_version,
      probe_house_name: d.probe_house_name,
      probe_second_name: d.probe_second_name,
      temperature_label: d.temperature_label,
    });
  } catch {
    // Non-fatal.
  }
}
