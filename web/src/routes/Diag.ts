import type { RouteCtx } from "../router";
import { h } from "../utils/dom";
import { api } from "../api/client";
import { poll, deviceInfo } from "../state/store";
import { fmt2, fmtAmps, fmtEnergyWh, fmtInt, fmtVolts } from "../utils/format";
import { toast } from "../components/Toast";
import { openDialog } from "../components/Dialog";
import { icon } from "../utils/icons";
import { getStrings } from "../i18n";
import type { Measurements, SystemInfo } from "../api/types";
import { routeCleanup } from "../utils/routeLifecycle";
import { buildMainsFrequencyBanner } from "../components/MainsFrequencyBanner";
import { buildPageHeader } from "../components/ui/pageHeader";
import { openFactoryResetDialog } from "../components/FactoryResetDialog";

export function mountDiag(ctx: RouteCtx): () => void {
  const { outlet, signal } = ctx;
  const T = getStrings();

  const head = buildPageHeader({
    title: T.diag.title,
    actions: [
      h(
        "button",
        {
          type: "button",
          class: "btn btn--ghost",
          onClick: copy,
        },
        icon("copy"),
        h("span", {}, T.diag.copy),
      ),
      h(
        "button",
        {
          type: "button",
          class: "btn btn--danger",
          onClick: () => openFactoryResetDialog({ signal }),
        },
        h("span", {}, T.settings.factoryReset),
      ),
      h(
        "button",
        {
          type: "button",
          class: "btn btn--danger",
          onClick: confirmReboot,
        },
        icon("reboot"),
        h("span", {}, T.settings.rebootBtn),
      ),
    ],
  });
  const mainsFreqBanner = buildMainsFrequencyBanner();
  const adcClipBanner = h("p", { class: "banner banner--warn", role: "status", hidden: true });
  const huntingBanner = h("p", { class: "banner banner--warn", role: "status", hidden: true });
  const selfTestCard = h("section", { class: "card" }, h("h2", { class: "card__title" }, T.diag.selfTestTitle));
  const selfTestBody = h("div", {});
  selfTestCard.append(selfTestBody);
  const triacCalEnable = h("input", { type: "checkbox" }) as HTMLInputElement;
  const triacW50 = h("input", { type: "number", min: "0", step: "1", class: "input" }) as HTMLInputElement;
  const triacW80 = h("input", { type: "number", min: "0", step: "1", class: "input" }) as HTMLInputElement;
  const triacCalCard = h(
    "section",
    { class: "card" },
    h("h2", { class: "card__title" }, T.diag.triacCalTitle),
    h("p", { class: "card__sub" }, T.diag.selfTestResistiveWarn),
    h("label", { class: "row", style: "gap:8px;align-items:center;" }, triacCalEnable, T.diag.triacCalEnable),
    h("label", {}, T.diag.triacCalDuty50, triacW50),
    h("label", {}, T.diag.triacCalDuty80, triacW80),
    h(
      "button",
      {
        type: "button",
        class: "btn btn--primary",
        style: "margin-top:8px;",
        onClick: async () => {
          const cfg = await api.getConfig({ signal });
          const base = { ...cfg.config };
          await api.putConfig(
            {
              ...base,
              triac_cal_enabled: triacCalEnable.checked,
              triac_calibration: [
                { duty_pct: 50, measured_w: Number(triacW50.value) || 0 },
                { duty_pct: 80, measured_w: Number(triacW80.value) || 0 },
                { duty_pct: 100, measured_w: Number(triacW80.value) || 0 },
              ],
            },
            { signal },
          );
          toast(T.saved, "success");
        },
      },
      T.diag.triacCalSave,
    ),
  );
  outlet.append(head, mainsFreqBanner.el, adcClipBanner, huntingBanner);

  // Raw measurements card (JSY-MK-194)
  const rawWrap = h("div", { class: "table-wrap" });
  const rawTable = h(
    "table",
    { class: "table" },
    h(
      "thead",
      {},
      h(
        "tr",
        {},
        h("th", {}, ""),
        h("th", { class: "num" }, T.home.importLabel),
        h("th", { class: "num" }, T.home.secondaryHeader),
        h("th", { class: "num" }, ""),
      ),
    ),
    h("tbody", {}),
  );
  rawWrap.append(rawTable);
  const rawCard = h(
    "section",
    { class: "card" },
    h("h2", { class: "card__title" }, T.diag.rawTitle),
    rawWrap,
  );

  // System info card
  const sysWrap = h("div", { class: "table-wrap" });
  const sysTable = h(
    "table",
    { class: "table" },
    h("tbody", {}),
  );
  sysWrap.append(sysTable);
  const sysCard = h(
    "section",
    { class: "card" },
    h("h2", { class: "card__title" }, T.diag.sysTitle),
    sysWrap,
  );

  outlet.append(selfTestCard, triacCalCard, rawCard, sysCard);

  void api.getConfig({ signal }).then((c) => {
    triacCalEnable.checked = !!c.config.triac_cal_enabled;
    const pts = c.config.triac_calibration ?? [];
    const p50 = pts.find((p) => p.duty_pct === 50);
    const p80 = pts.find((p) => p.duty_pct === 80);
    if (p50) triacW50.value = String(p50.measured_w);
    if (p80) triacW80.value = String(p80.measured_w);
  });

  async function refreshSelfTest() {
    const hinfo = await api.getHealth({ signal });
    const st = hinfo.self_test;
    selfTestBody.replaceChildren();
    if (st?.pending) {
      selfTestBody.append(
        h("p", { class: "card__sub" }, T.diag.selfTestPending),
        h("p", { class: "card__sub" }, T.diag.selfTestResistiveWarn),
        h(
          "div",
          { class: "row", style: "gap:8px;margin-top:8px;" },
          h(
            "button",
            {
              type: "button",
              class: "btn btn--primary",
              onClick: async () => {
                await api.postHealthSelfTestRun({ signal });
                toast(T.saved, "success");
                void refreshSelfTest();
              },
            },
            T.diag.selfTestRun,
          ),
          h(
            "button",
            {
              type: "button",
              class: "btn btn--ghost",
              onClick: async () => {
                await api.postHealthSelfTestSkip({ signal });
                toast(T.saved, "success");
                void refreshSelfTest();
              },
            },
            T.diag.selfTestSkip,
          ),
        ),
      );
    } else if (st?.results) {
      const r = st.results;
      const ok = T.on;
      const dash = T.unknown;
      selfTestBody.append(
        h(
          "dl",
          { class: "source-summary" },
          h("dt", {}, T.diag.selfTestZc),
          h("dd", {}, `${r.zc_ok ? ok : dash} (${r.zc_edges_per_sec ?? 0}/s)`),
          h("dt", {}, T.diag.selfTestTriac),
          h("dd", {}, r.triac_ok ? ok : dash),
          h("dt", {}, T.diag.selfTestSource),
          h("dd", {}, r.source_ok ? ok : dash),
        ),
      );
    }
  }

  let lastMeasurements: Measurements | null = null;
  let lastSystem: SystemInfo | null = null;

  async function copy() {
    const lines: string[] = [];
    if (lastMeasurements) {
      lines.push(T.diag.copySectionMeasurements);
      lines.push(JSON.stringify(lastMeasurements, null, 2));
    }
    if (lastSystem) {
      lines.push(`\n${T.diag.copySectionSystem}`);
      lines.push(JSON.stringify(lastSystem, null, 2));
    }
    if (deviceInfo.get()) {
      lines.push(`\n${T.diag.copySectionDevice}`);
      lines.push(JSON.stringify(deviceInfo.get(), null, 2));
    }
    const text = lines.join("\n");
    try {
      await navigator.clipboard.writeText(text);
      toast(T.copyOk, "success");
    } catch {
      toast(T.copyErr, "error");
    }
  }

  function confirmReboot() {
    openDialog({
      title: T.settings.rebootConfirmTitle,
      body: h("p", {}, T.settings.rebootConfirmBody),
      actions: [
        { label: T.cancel, kind: "ghost", onClick: () => {} },
        {
          label: T.settings.rebootBtn,
          kind: "danger",
          onClick: async () => {
            try {
              await api.reboot({ signal });
              toast(T.settings.rebooting, "info", 6000);
            } catch {
              toast(T.saveError, "error");
            }
          },
        },
      ],
    });
  }

  return routeCleanup(signal, (scope) => {
  void refreshSelfTest();
  scope.trackPoll(poll(
    async () => {
      const { config } = await api.getConfig({ signal });
      mainsFreqBanner.setWarning(config.mains_frequency_warning, {
        effectiveHz: config.mains_frequency_effective_hz,
        source: config.mains_frequency_source,
        installCountry: config.install_country,
      });
    },
    15000,
    20000,
  ));

  scope.trackPoll(poll(
    async () => {
      const m = await api.getMeasurements({ signal });
      lastMeasurements = m;
      adcClipBanner.hidden = !m.diagnostics?.adc_clipping;
      if (!adcClipBanner.hidden) adcClipBanner.textContent = T.diag.adcClippingBanner;
      huntingBanner.hidden = !m.diagnostics?.regulation_hunting;
      if (!huntingBanner.hidden) huntingBanner.textContent = T.diag.huntingBanner;
      const r = m.raw_meter;
      const houseLabel = deviceInfo.get()?.probe_house_name || T.home.importLabel;
      const probeLabel = deviceInfo.get()?.probe_second_name || T.home.secondaryHeader;
      const tbody = rawTable.querySelector("tbody")!;
      tbody.replaceChildren(
        h("tr", {}, h("td", { colspan: "4", style: "background:var(--c-surface-2);font-weight:600;" }, houseLabel)),
        rawRow(T.diag.voltage, fmtVolts(r.voltage_house_v), fmtVolts(r.voltage_second_v), "V"),
        rawRow(T.diag.current, fmtAmps(r.current_house_a), fmtAmps(r.current_second_a), "A"),
        rawRow(T.diag.power, fmtInt(r.house_net_power_w), fmtInt(r.second_net_power_w), "W"),
        rawRow(T.diag.pf, fmt2(r.pf_house), fmt2(r.pf_second), ""),
        rawRow(T.diag.energyImport,
          fmtEnergyWh(m.house.energy_total_import_wh).value,
          fmtEnergyWh(m.second?.energy_total_import_wh ?? 0).value,
          fmtEnergyWh(m.house.energy_total_import_wh).unit),
        rawRow(T.diag.energyExport,
          fmtEnergyWh(m.house.energy_total_export_wh).value,
          fmtEnergyWh(m.second?.energy_total_export_wh ?? 0).value,
          fmtEnergyWh(m.house.energy_total_export_wh).unit),
        h("tr", {}, h("td", { colspan: "4", style: "background:var(--c-surface-2);font-weight:600;" }, probeLabel)),
        h("tr", {},
          h("td", {}, T.diag.frequency),
          h("td", { class: "num", colspan: "2" }, fmt2(r.freq_hz)),
          h("td", {}, T.diag.unitHz),
        ),
      );
    },
    2000,
    5000,
  ));

  scope.trackPoll(poll(
    async () => {
      const s = await api.getSystem({ signal });
      lastSystem = s;
      const tbody = sysTable.querySelector("tbody")!;
      const upH = Math.floor(s.uptime_hours);
      const upMin = Math.round((s.uptime_hours - upH) * 60);
      tbody.replaceChildren(
        sysRow(T.diag.uptime, `${upH} h ${upMin} min`),
        sysRow(T.diag.rssi, `${s.wifi_rssi_dbm} dBm`),
        sysRow(T.diag.ssid, s.ssid),
        sysRow(T.diag.bssid, s.wifi_bssid),
        sysRow(T.diag.mac, s.mac),
        sysRow(T.diag.ip, s.ip),
        sysRow(T.diag.gateway, s.gateway),
        sysRow(T.diag.subnet, s.subnet),
        sysRow(T.diag.dns, s.dns ?? "—"),
        sysRow(T.diag.meteringTask, `${s.metering_task_ms.join(" / ")} ms`),
        sysRow(T.diag.loopTask, `${s.loop_task_ms.join(" / ")} ms`),
        sysRow(T.diag.eeprom, `${s.eeprom_used_percent} %`),
        sysRow(T.diag.irq, String(s.irq_10ms_raw_vs_in ?? "—")),
      );
    },
    5000,
    8000,
  ));
  });
}

function rawRow(label: string, vHouse: string, vSecond: string, unit: string) {
  return h(
    "tr",
    {},
    h("td", {}, label),
    h("td", { class: "num" }, vHouse),
    h("td", { class: "num" }, vSecond),
    h("td", {}, unit),
  );
}

function sysRow(label: string, value: string) {
  return h(
    "tr",
    {},
    h("td", {}, label),
    h("td", { class: "num" }, value),
  );
}
