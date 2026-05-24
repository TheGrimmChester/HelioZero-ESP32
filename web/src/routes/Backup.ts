import type { RouteCtx } from "../router";
import { h } from "../utils/dom";
import { buildSectionTitleWithHelp } from "../components/FieldHelp";
import { icon } from "../utils/icons";
import { api } from "../api/client";
import { toast } from "../components/Toast";
import { getStrings } from "../i18n";
import { withBase } from "../paths";
import {
  backupDownloadFilename,
  buildBackup,
  downloadJsonFile,
} from "../utils/backupFormat";
import { confirmRestoreBackupFromFile } from "../utils/backupApply";
import { buildPageHeader } from "../components/ui/pageHeader";

export async function mountBackup(ctx: RouteCtx) {
  const { outlet, signal } = ctx;
  const T = getStrings();

  outlet.append(
    buildPageHeader({
      title: T.backup.title,
      actions: [
        h(
          "a",
          {
            href: withBase("/settings/general"),
            class: "btn btn--ghost",
            "data-route": "true",
          },
          T.backup.backToSettings,
        ),
      ],
    }),
  );

  outlet.append(
    h(
      "section",
      { class: "card" },
      h("p", {}, T.backup.intro),
    ),
  );

  outlet.append(
    h(
      "section",
      { class: "card" },
      buildSectionTitleWithHelp(T.backup.sectionSecurity, "backup", "sectionSecurity"),
      h(
        "p",
        { class: "field__hint", style: "display:flex;gap:10px;align-items:flex-start;" },
        icon("alert"),
        h("span", {}, T.backup.security),
      ),
    ),
  );

  const exportBtn = h(
    "button",
    {
      type: "button",
      class: "btn btn--primary",
      onClick: () => void doExport(),
    },
    icon("download"),
    h("span", {}, T.backup.exportBtn),
  );

  outlet.append(
    h(
      "section",
      { class: "card" },
      buildSectionTitleWithHelp(T.backup.sectionExport, "backup", "sectionExport"),
      exportBtn,
    ),
  );

  const fileInput = h("input", {
    type: "file",
    accept: "application/json,.json",
    hidden: true,
  }) as HTMLInputElement;

  fileInput.addEventListener("change", () => {
    const f = fileInput.files?.[0];
    fileInput.value = "";
    if (f) void confirmRestoreBackupFromFile(f, signal);
  });

  const importBtn = h(
    "button",
    {
      type: "button",
      class: "btn btn--ghost",
      onClick: () => fileInput.click(),
    },
    icon("upload"),
    h("span", {}, T.backup.importBtn),
  );

  outlet.append(
    h(
      "section",
      { class: "card" },
      buildSectionTitleWithHelp(T.backup.sectionImport, "backup", "sectionImport"),
      fileInput,
      importBtn,
    ),
  );

  async function doExport() {
    try {
      const [cfgEnv, actEnv, timeInfo, wifiInfo] = await Promise.all([
        api.getConfig({ signal }),
        api.getActionsConfig({ signal }),
        api.getTime({ signal }),
        api.getWifi({ signal }),
      ]);
      const doc = buildBackup(
        cfgEnv.config,
        actEnv,
        { tz: timeInfo.tz, ntp1: timeInfo.ntp1, ntp2: timeInfo.ntp2 },
        { ssid: wifiInfo.ssid, password: wifiInfo.password ?? "" },
      );
      downloadJsonFile(backupDownloadFilename(), doc);
    } catch (e) {
      if ((e as DOMException)?.name === "AbortError") return;
      console.error(e);
      toast(T.backup.exportError, "error");
    }
  }
}
