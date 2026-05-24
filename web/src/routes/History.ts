import type { RouteCtx } from "../router";
import { h } from "../utils/dom";
import { api } from "../api/client";
import { poll, deviceInfo, localePref } from "../state/store";
import { buildChart } from "../components/Chart";
import { getStrings } from "../i18n";
import { routeCleanup } from "../utils/routeLifecycle";
import { openDialog } from "../components/Dialog";
import { toast } from "../components/Toast";
import { go } from "../router";
import type { HistoryEnergyDaily, HistoryPower } from "../api/types";
import { fmtEnergyWh, isProbeTemperatureReading } from "../utils/format";
import { powerChartSeriesSpecs } from "../utils/chartChannelColors";
import { energyDayIsoDates } from "../utils/historyEnergy";
import {
  buildPowerTimeAxisHours,
  buildPowerTimeAxisSeconds,
  formatPowerAxisHours,
  formatPowerAxisSeconds,
  padSeries,
  powerHasSignal,
} from "../utils/historyPower";
import { buildPageHeader } from "../components/ui/pageHeader";
import { pmqttBindingsMissing } from "../utils/pmqttBindings";

function monthShortTick(m: number): string {
  const loc = localePref.get() === "fr" ? "fr-FR" : "en-US";
  const mm = Math.max(0, Math.min(11, m));
  return new Date(2000, mm, 1).toLocaleDateString(loc, { month: "short" });
}

function historySeriesHasProbeTemp(series: number[]): boolean {
  const valid = series.filter((v) => isProbeTemperatureReading(v));
  if (valid.length === 0) return false;
  if (valid.every((v) => v === 0)) return false;
  return true;
}

function resolveProbeTempVisible(
  power: HistoryPower | null,
  liveTempC: number | undefined,
): boolean {
  if (isProbeTemperatureReading(liveTempC)) return true;
  if (power && isProbeTemperatureReading(power.temperature_now_c)) return true;
  if (power?.window === "48h") {
    return historySeriesHasProbeTemp(power.temperature_series_c || []);
  }
  return false;
}

const ENERGY_CHART_MAX_POINTS = 52;
const ENERGY_TABLE_DAYS = 31;

function downsampleDailyWh(values: number[], maxPoints: number): number[] {
  if (values.length <= maxPoints) return values;
  const out: number[] = [];
  const step = values.length / maxPoints;
  for (let i = 0; i < maxPoints; i++) {
    const start = Math.floor(i * step);
    const end = Math.min(values.length, Math.floor((i + 1) * step));
    let sum = 0;
    let n = 0;
    for (let j = start; j < end; j++) {
      sum += values[j] ?? 0;
      n++;
    }
    out.push(n > 0 ? Math.round(sum / n) : 0);
  }
  return out;
}

export function mountHistory(ctx: RouteCtx): () => void {
  const { outlet, signal } = ctx;
  const T = getStrings();

  let powerWindow: "10m" | "48h" = "48h";
  let maxPoints = 200;
  let lastPower: HistoryPower | null = null;
  let lastEnergy: HistoryEnergyDaily | null = null;
  let measurementSourceOk: boolean | null = null;
  let liveTempC: number | undefined;
  let deviceDateTime: string | undefined;
  let tempTabVisible = false;

  const windowSelect = h("select", { class: "input" }) as HTMLSelectElement;
  for (const opt of [
    { v: "48h", l: T.history.window48h },
    { v: "10m", l: T.history.window10m },
  ] as const) {
    const o = h("option", { value: opt.v }, opt.l) as HTMLOptionElement;
    if (opt.v === powerWindow) o.selected = true;
    windowSelect.append(o);
  }
  const maxPointsInput = h("input", {
    class: "input",
    type: "number",
    min: "32",
    max: "1200",
    value: String(maxPoints),
    style: "width:6rem;",
  }) as HTMLInputElement;

  const powerTitleEl = h("h3", { class: "card__title" }, T.history.chart48hPowerTitle);

  const pmqttBindingsBanner = h("p", {
    class: "banner banner--warn",
    role: "alert",
    hidden: true,
  });
  const noDataBanner = h("p", {
    class: "banner banner--warn",
    role: "status",
    hidden: true,
  });
  const settingsLink = h(
    "button",
    {
      type: "button",
      class: "btn btn--ghost",
      style: "margin-top:8px;",
      onClick: () => void go("/settings/metering"),
    },
    T.history.openSettings,
  );
  noDataBanner.append(
    h("span", {}, T.history.noDataYet),
    h("br"),
    settingsLink,
  );

  const powerChart = buildChart({
    title: "",
    yLabel: "W",
    y2Label: "VA",
    height: 260,
    series: powerChartSeriesSpecs({
      houseActive: T.history.legendHouse,
      triacActive: T.history.legendTriac,
      houseApparent: `${T.history.legendHouse} S`,
      triacApparent: `${T.history.legendTriac} S`,
    }),
    data: [[0], [0], [0], [0], [0]] as unknown as [
      number[],
      number[],
      number[],
      number[],
      number[],
    ],
    xFormat: (v) => formatPowerAxisHours(v),
  });

  const tempChart = buildChart({
    title: T.history.chart48hTempTitle,
    yLabel: "°C",
    height: 220,
    intLegend: false,
    series: [{ label: T.home.temperature, color: "var(--c-temp)", unit: "°C" }],
    data: [[0], [0]] as unknown as [number[], number[]],
    xFormat: (h) => formatPowerAxisHours(h),
  });
  const energyChart = buildChart({
    title: T.history.chart1yEnergyTitle,
    yLabel: "Wh",
    height: 180,
    intLegend: false,
    series: [{ label: T.history.energySeries, color: "var(--c-energy)", unit: "Wh" }],
    data: [[0], [0]] as unknown as [number[], number[]],
    xFormat: (idx) => monthShortTick(Math.floor(idx) % 12),
  });

  const energyTableBody = h("tbody", {});

  const tabs = ["pw", "temp", "year"] as const;
  type Tab = (typeof tabs)[number];
  let activeTab: Tab = "pw";

  const tabsEl = h("div", {
    class: "tabs history-tabs",
    role: "tablist",
    "aria-label": T.history.tabsAria,
  });
  const panelsRoot = h("div", { class: "history-tabpanels" });
  const panes: Record<Tab, HTMLElement> = {
    pw: h(
      "section",
      {
        class: "card history-tabpanel",
        role: "tabpanel",
        id: "history-panel-pw",
      },
      powerTitleEl,
      noDataBanner,
      powerChart.el,
    ),
    temp: h(
      "section",
      {
        class: "card history-tabpanel",
        role: "tabpanel",
        id: "history-panel-temp",
      },
      tempChart.el,
    ),
    year: h(
      "section",
      {
        class: "card history-tabpanel",
        role: "tabpanel",
        id: "history-panel-year",
      },
      energyChart.el,
      h("h3", { class: "card__sub" }, T.history.energyTableTitle),
      h(
        "div",
        { class: "table-wrap history-energy-table-wrap" },
        h(
          "table",
          { class: "table" },
          h(
            "thead",
            {},
            h(
              "tr",
              {},
              h("th", {}, T.history.colDay),
              h("th", { class: "num" }, T.history.colDeltaWh),
            ),
          ),
          energyTableBody,
        ),
      ),
    ),
  };
  const labels: Record<Tab, string> = {
    pw: T.history.tab48hPower,
    temp: T.history.tab48hTemp,
    year: T.history.tab1yEnergy,
  };
  const panelIds: Record<Tab, string> = {
    pw: "history-panel-pw",
    temp: "history-panel-temp",
    year: "history-panel-year",
  };

  const buttons: Record<Tab, HTMLButtonElement> = {} as never;
  for (const tab of tabs) {
    const btn = h(
      "button",
      {
        type: "button",
        class: "tabs__btn",
        role: "tab",
        id: `history-tab-${tab}`,
        "aria-controls": panelIds[tab],
        "aria-selected": tab === "pw" ? "true" : "false",
        onClick: () => selectTab(tab),
      },
      labels[tab],
    );
    buttons[tab] = btn;
    tabsEl.append(btn);
  }
  buttons.temp.hidden = true;

  for (const tab of tabs) {
    panelsRoot.append(panes[tab]);
    panes[tab].hidden = tab !== "pw";
  }

  function setTempTabVisible(visible: boolean) {
    tempTabVisible = visible;
    buttons.temp.hidden = !visible;
    if (!visible && activeTab === "temp") selectTab("pw");
  }

  function selectTab(tab: Tab) {
    if (tab === "temp" && !tempTabVisible) tab = "pw";
    activeTab = tab;
    for (const t of tabs) {
      const isActive = t === tab;
      buttons[t].setAttribute("aria-selected", isActive ? "true" : "false");
      panes[t].hidden = !isActive;
    }
    queueMicrotask(() => {
      if (tab === "pw") powerChart.resize();
      else if (tab === "temp") tempChart.resize();
      else energyChart.resize();
    });
  }

  function syncTempTabVisibility() {
    setTempTabVisible(resolveProbeTempVisible(lastPower, liveTempC));
  }

  function syncPowerChartLabels() {
    const d = deviceInfo.get();
    const house = d?.probe_house_name || T.history.legendHouse;
    const triac = d?.probe_second_name || T.history.legendTriac;
    powerChart.setSeriesLabels([house, triac, `${house} S`, `${triac} S`]);
  }
  syncPowerChartLabels();

  function updateNoDataBanner(j: HistoryPower | null) {
    const show =
      measurementSourceOk === false ||
      (j !== null && !powerHasSignal(j.house_active_w || [], j.triac_active_w || []));
    noDataBanner.hidden = !show;
  }

  function applyPower(j: HistoryPower) {
    lastPower = j;
    const is10m = j.window === "10m" || powerWindow === "10m";
    powerTitleEl.textContent = is10m
      ? T.history.chart10mPowerTitle
      : T.history.chart48hPowerTitle;

    const m = j.house_active_w || [];
    const tr = j.triac_active_w || [];
    const vaM = j.house_apparent_va || [];
    const vaT = j.triac_apparent_va || [];
    const n = Math.max(m.length, tr.length, vaM.length, vaT.length, 1);
    const periodS = j.sample_period_s || (is10m ? 2 : 300);
    const xs = is10m
      ? buildPowerTimeAxisSeconds(n, periodS)
      : buildPowerTimeAxisHours(n, periodS);

    powerChart.setXFormat((v) =>
      is10m ? formatPowerAxisSeconds(v) : formatPowerAxisHours(v),
    );
    powerChart.setData([
      xs,
      padSeries(m, n),
      padSeries(tr, n),
      padSeries(vaM, n),
      padSeries(vaT, n),
    ] as unknown as [number[], number[], number[], number[], number[]]);

    updateNoDataBanner(j);
    syncTempTabVisibility();

    const t = j.temperature_series_c || [];
    if (tempTabVisible && j.window === "48h" && historySeriesHasProbeTemp(t)) {
      const xst = buildPowerTimeAxisHours(t.length, periodS);
      tempChart.setData([xst, t] as unknown as [number[], number[]]);
    }
  }

  function applyEnergy(j: HistoryEnergyDaily) {
    lastEnergy = j;
    const arr = j.delta_wh_per_day || [];
    const iso =
      j.day_dates_iso?.length === arr.length
        ? j.day_dates_iso
        : energyDayIsoDates(
            arr.length,
            j.reference_date_iso,
            deviceDateTime,
          );
    const chartVals = downsampleDailyWh(arr, ENERGY_CHART_MAX_POINTS);
    const xs = Array.from({ length: chartVals.length }, (_, i) => i);
    energyChart.setData([xs, chartVals] as unknown as [number[], number[]]);
    energyTableBody.replaceChildren();
    const tableStart = Math.max(0, arr.length - ENERGY_TABLE_DAYS);
    for (let i = tableStart; i < arr.length; i++) {
      const dayLabel = iso[i] ?? "—";
      const e = fmtEnergyWh(arr[i]);
      energyTableBody.append(
        h(
          "tr",
          {},
          h("td", {}, dayLabel),
          h("td", { class: "num" }, `${e.value} ${e.unit}`),
        ),
      );
    }
  }

  function downloadCsv() {
    const rows: string[] = ["section,key,value"];
    if (lastPower) {
      rows.push(`power,window,${lastPower.window || powerWindow}`);
      (lastPower.house_active_w || []).forEach((v, i) =>
        rows.push(`power,house_active_w_${i},${v}`),
      );
      (lastPower.triac_active_w || []).forEach((v, i) =>
        rows.push(`power,triac_active_w_${i},${v}`),
      );
    }
    if (lastEnergy) {
      const iso = lastEnergy.day_dates_iso || [];
      (lastEnergy.delta_wh_per_day || []).forEach((v, i) => {
        rows.push(`energy,date_iso,${iso[i] ?? ""}`);
        rows.push(`energy,delta_wh_day_${i},${v}`);
      });
    }
    const blob = new Blob([rows.join("\n")], { type: "text/csv" });
    const a = h("a", {
      href: URL.createObjectURL(blob),
      download: "helio-zero-history.csv",
    }) as HTMLAnchorElement;
    a.click();
    URL.revokeObjectURL(a.href);
  }

  function confirmReset() {
    openDialog({
      title: T.history.resetHistory,
      body: h("p", {}, T.history.resetHistoryConfirm),
      actions: [
        { label: T.cancel, kind: "ghost", onClick: () => {} },
        {
          label: T.reset,
          kind: "danger",
          onClick: async () => {
            await api.resetHistory({ signal });
            toast(T.history.resetDone, "success");
            await loadPower();
            await loadEnergy();
          },
        },
      ],
    });
  }

  const toolbar = h(
    "div",
    { class: "row spread", style: "flex-wrap:wrap;gap:8px;margin-bottom:12px;" },
    h("label", { class: "field" }, T.history.windowLabel, windowSelect),
    h("label", { class: "field" }, T.history.maxPoints, maxPointsInput),
    h(
      "button",
      { type: "button", class: "btn btn--ghost", onClick: () => void loadPower() },
      T.retry,
    ),
    h(
      "button",
      { type: "button", class: "btn btn--ghost", onClick: downloadCsv },
      T.history.exportCsv,
    ),
    h(
      "button",
      { type: "button", class: "btn btn--danger", onClick: confirmReset },
      T.history.resetHistory,
    ),
  );

  async function loadPower() {
    powerWindow = windowSelect.value === "10m" ? "10m" : "48h";
    maxPoints = Math.max(32, Math.min(1200, parseInt(maxPointsInput.value, 10) || 200));
    maxPointsInput.value = String(maxPoints);
    try {
      const j = await api.getHistoryPower(powerWindow, maxPoints, { signal });
      applyPower(j);
    } catch {
      toast(T.history.loadError, "error");
    }
  }

  async function loadEnergy() {
    try {
      const j = await api.getHistoryEnergyDaily({ signal });
      applyEnergy(j);
    } catch {
      toast(T.history.loadError, "error");
    }
  }

  maxPointsInput.addEventListener("change", () => void loadPower());

  outlet.append(
    buildPageHeader({ title: T.history.title }),
    pmqttBindingsBanner,
    toolbar,
    tabsEl,
    panelsRoot,
  );

  const unsubDevice = deviceInfo.subscribe(() => syncPowerChartLabels());

  return routeCleanup(signal, (scope) => {
    scope.onUnmount(() => {
      unsubDevice();
      powerChart.destroy();
      tempChart.destroy();
      energyChart.destroy();
    });

    async function refreshLiveContext() {
      try {
        const [st, m] = await Promise.all([
          api.getState({ signal }),
          api.getMeasurements({ signal }),
        ]);
        liveTempC =
          typeof st.temperature_c === "number" ? st.temperature_c : undefined;
        deviceDateTime = m.date_valid ? m.date : undefined;
        if (lastEnergy) applyEnergy(lastEnergy);
      } catch {
        liveTempC = undefined;
      }
      syncTempTabVisibility();
    }

    void (async () => {
      try {
        const [health, cfgRes] = await Promise.all([
          api.getHealth({ signal }),
          api.getConfig({ signal }),
        ]);
        measurementSourceOk = health.source_ok;
        pmqttBindingsBanner.hidden = !pmqttBindingsMissing(cfgRes.config);
        if (!pmqttBindingsBanner.hidden) {
          pmqttBindingsBanner.textContent = T.home.pmqttBindingsMissing;
        }
        updateNoDataBanner(lastPower);
      } catch {
        measurementSourceOk = null;
        pmqttBindingsBanner.hidden = true;
      }
    })();

    void refreshLiveContext();
    void loadPower();
    void loadEnergy();

    const powerPollMs = () => (powerWindow === "10m" ? 12_000 : 60_000);

    let powerPollTimer: ReturnType<typeof poll> | null = null;
    function restartPowerPoll() {
      powerPollTimer?.stop();
      powerPollTimer = poll(
        async () => {
          await loadPower();
        },
        powerPollMs(),
        30_000,
        { immediate: false },
      );
      scope.trackPoll(powerPollTimer);
    }
    restartPowerPoll();
    windowSelect.addEventListener("change", () => {
      void loadPower();
      restartPowerPoll();
    });

    scope.trackPoll(
      poll(
        async () => {
          await refreshLiveContext();
        },
        60_000,
        30_000,
        { immediate: false },
      ),
    );

    scope.trackPoll(
      poll(
        async () => {
          await loadEnergy();
        },
        300_000,
        30_000,
        { immediate: false },
      ),
    );
  });
}
