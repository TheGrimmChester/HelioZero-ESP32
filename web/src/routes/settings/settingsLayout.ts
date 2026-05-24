import type { RouteCtx } from "../../router";
import { h } from "../../utils/dom";
import { api, ApiError } from "../../api/client";
import type { MqttTestResponse } from "../../api/types";
import { toast } from "../../components/Toast";
import { isProbeTemperatureReading } from "../../utils/format";
import {
  applyTemperatureFieldVisibility,
  collectAdvancedTemperaturePatch,
  collectIdentityTemperaturePatch,
} from "../../utils/temperatureSettingsVisibility";
import type { RouterConfig } from "../../api/types";
import { getStrings } from "../../i18n";
import { setUnsavedGuard } from "../../navigationGuard";
import { withBase } from "../../paths";
import { localePref } from "../../state/store";
import {
  applyInstallCountryToConfig,
  buildInstallCountrySection,
  readInstallCountryState,
} from "../../components/InstallCountryFields";
import { buildTimezoneCountryField } from "../../components/TimezoneCountryField";
import { resolveIntlTimeZone } from "../../utils/zonedDateTime";
import { buildVacationEndField } from "../../components/VacationEndField";
import { buildPmqttSetupFlow } from "../../components/PmqttSetupFlow";
import { buildMeasurementSourceSummary } from "../../components/MeasurementSourceSummary";
import { buildNetworkStatusSummary } from "../../components/NetworkStatusSummary";
import { displayStoredIp } from "../../utils/networkIp";
import { effectiveMqttDeviceName } from "../../utils/mqttDeviceName";
import {
  attachCardAutosave,
  attachSettingsCardAutosave,
  type SettingsCardAutosave,
} from "./cardAutosave";
import { wrapSwitchWithHelp } from "../../components/FieldHelp";
import { settingsSwitchLabel } from "../../utils/settingsSwitch";
import { buildTempoRteStatus } from "../../components/TempoRteStatus";
import { numberRow, passwordRow, textRow, validateIp } from "./formRows";
import { buildAdvancedSection } from "./cards/advancedCard";
import { buildMeasurementSettingsCard } from "./cards/measurementCard";
import { buildMqttSettingsCard, buildNetworkSettingsCard } from "./cards/networkCard";
import { settingsSection } from "./cards/section";
import { buildSettingsChrome } from "./settingsChrome";
import { parseSettingsSection, type SettingsSection } from "./settingsPaths";
import { setSettingsLayoutHandlers } from "./settingsRouterBridge";

const H = { helpScope: "settings" as const };

export async function mountSettingsLayout(ctx: RouteCtx): Promise<() => void> {
  const { outlet, signal } = ctx;
  const T = getStrings();
  const initialSection = parseSettingsSection(ctx.path);

  const chrome = buildSettingsChrome(signal);
  const loading = h("p", { class: "empty" }, T.loading);
  outlet.append(chrome.root, loading);

  let cfg: RouterConfig;
  let deviceUid = "";
  let tempSensorPresent = false;
  try {
    const [env, device, state] = await Promise.all([
      api.getConfig({ signal }),
      api.getDevice({ signal }),
      api.getState({ signal }),
    ]);
    cfg = env.config;
    deviceUid = device.device_uid?.trim() ?? "";
    tempSensorPresent = isProbeTemperatureReading(state.temperature_c);
  } catch (e) {
    if ((e as DOMException)?.name === "AbortError") return () => {};
    loading.textContent = T.status.error + " — " + T.retry;
    return () => {};
  }
  loading.remove();

  const dhcpInput = h("input", {
    type: "checkbox",
    checked: cfg.dhcp_on,
    onChange: () => updateNetworkDisabled(),
  });
  const dhcpRow = wrapSwitchWithHelp(
    settingsSwitchLabel(dhcpInput as HTMLInputElement, T.settings.dhcp),
    "settings",
    "dhcp",
  );

  const ipFixed = textRow(
    "ip_fixed",
    T.settings.ipFixed,
    displayStoredIp(cfg.ip_fixed),
    T.settings.requireReboot,
    { ...H, helpKey: "ip_fixed" },
  );
  const gateway = textRow("gateway", T.settings.gateway, displayStoredIp(cfg.gateway), T.settings.boxHint, {
    ...H,
    helpKey: "gateway",
  });
  const subnet = textRow("subnet_mask", T.settings.subnet, displayStoredIp(cfg.subnet_mask), "", {
    ...H,
    helpKey: "subnet_mask",
  });
  const dns = textRow("dns", T.settings.dns, displayStoredIp(cfg.dns), T.settings.boxHint, { ...H, helpKey: "dns" });

  const router = textRow("router_name", T.settings.routerName, cfg.router_name, "", { ...H, helpKey: "router_name" });
  const probeHouse = textRow("probe_house_name", T.settings.probeHouse, cfg.probe_house_name, "", {
    ...H,
    helpKey: "probe_house_name",
  });
  const probeSecond = textRow("probe_second_name", T.settings.probeSecond, cfg.probe_second_name, "", {
    ...H,
    helpKey: "probe_second_name",
  });
  const probeTemp = textRow("temperature_label", T.settings.probeTemperature, cfg.temperature_label, "", {
    ...H,
    helpKey: "temperature_label",
  });

  const mqttPeriod = numberRow(
    "mqtt_repeat_sec",
    T.settings.mqttPeriod,
    cfg.mqtt_repeat_sec,
    T.settings.mqttPeriodHint,
    () => updateMqttDisabled(),
    { ...H, helpKey: "mqtt_repeat_sec" },
  );
  const mqttIp = textRow("mqtt_ip", T.settings.mqttIp, cfg.mqtt_ip, "", { ...H, helpKey: "mqtt_ip" });
  const mqttPort = numberRow("mqtt_port", T.settings.mqttPort, cfg.mqtt_port, "", undefined, {
    ...H,
    helpKey: "mqtt_port",
  });
  const mqttUser = textRow("mqtt_user", T.settings.mqttUser, cfg.mqtt_user, "", { ...H, helpKey: "mqtt_user" });
  const mqttPwd = passwordRow("mqtt_password", T.settings.mqttPwd, cfg.mqtt_password, "", {
    ...H,
    helpKey: "mqtt_password",
  });
  const mqttPrefix = textRow("mqtt_prefix", T.settings.mqttPrefix, cfg.mqtt_prefix, T.settings.mqttPrefixHint, {
    ...H,
    helpKey: "mqtt_prefix",
  });
  const resolveMqttDeviceName = (raw: string) => effectiveMqttDeviceName(raw, deviceUid);
  const mqttDevice = textRow(
    "mqtt_device_name",
    T.settings.mqttDevice,
    resolveMqttDeviceName(cfg.mqtt_device_name),
    "",
    {
      ...H,
      helpKey: "mqtt_device_name",
    },
  );
  const vacationInput = h("input", {
    type: "checkbox",
    checked: !!cfg.vacation_enabled,
  }) as HTMLInputElement;
  const vacationRow = wrapSwitchWithHelp(
    settingsSwitchLabel(vacationInput, T.settings.vacationEnabled),
    "settings",
    "vacation_enabled",
  );
  let deviceTz = "";
  const getVacationTimeZone = () =>
    resolveIntlTimeZone(deviceTz, cfg.install_country || "FR", cfg.install_country_variant);
  const vacationEnd = buildVacationEndField({
    T,
    initialEpoch: cfg.vacation_end_epoch ?? 0,
    getTimeZone: getVacationTimeZone,
    helpScope: "settings",
    helpKey: "vacation_end_epoch",
  });
  const triacStaleInput = h("input", {
    type: "checkbox",
    checked: !!cfg.triac_off_when_source_stale,
  }) as HTMLInputElement;
  const triacStaleRow = wrapSwitchWithHelp(
    settingsSwitchLabel(triacStaleInput, T.settings.triacOffWhenSourceStale),
    "settings",
    "triac_off_when_source_stale",
  );
  const triacStaleHint = h("p", { class: "field__hint" }, T.settings.triacOffWhenSourceStaleHint);
  const heaterBackoffInput = h("input", {
    type: "checkbox",
    checked: !!cfg.triac_backoff_when_heater_idle,
  }) as HTMLInputElement;
  const heaterBackoffRow = wrapSwitchWithHelp(
    settingsSwitchLabel(heaterBackoffInput, T.settings.triacBackoffWhenHeaterIdle),
    "settings",
    "triac_backoff_when_heater_idle",
  );
  const heaterBackoffHint = h("p", { class: "field__hint" }, T.settings.triacBackoffWhenHeaterIdleHint);
  const maxRouted = numberRow("max_routed_w", T.settings.maxRoutedW, cfg.max_routed_w ?? 0, "", undefined, {
    ...H,
    helpKey: "max_routed_w",
  });
  const mqttJsonInput = h("input", {
    type: "checkbox",
    checked: !!cfg.mqtt_json_commands,
  }) as HTMLInputElement;
  const mqttJsonRow = wrapSwitchWithHelp(
    settingsSwitchLabel(mqttJsonInput, T.settings.mqttJsonCommands),
    "settings",
    "mqtt_json_commands",
  );
  const mqttJsonHint = h("p", { class: "field__hint" }, T.settings.mqttJsonCommandsHint);

  const calibU = numberRow("calib_u", T.settings.calibU, cfg.calib_u, T.settings.calibTypical, undefined, {
    ...H,
    helpKey: "calib_u",
  });
  const calibI = numberRow("calib_i", T.settings.calibI, cfg.calib_i, T.settings.calibTypical, undefined, {
    ...H,
    helpKey: "calib_i",
  });
  const pmqttTopic = textRow("pmqtt_topic", T.settings.pmqttTopic, cfg.pmqtt_topic ?? "", "", {
    ...H,
    helpKey: "pmqtt_topic",
  });
  const pmqttFlow = buildPmqttSetupFlow(
    T,
    cfg.pmqtt_bindings ?? [],
    {
      mqtt_ip: cfg.mqtt_ip ?? "",
      mqtt_port: cfg.mqtt_port ?? 1883,
      mqtt_user: cfg.mqtt_user ?? "",
      mqtt_password: cfg.mqtt_password ?? "",
    },
    () => {},
  );
  const triacTempCap = numberRow(
    "triac_override_max_temp_c",
    T.settings.triacOverrideMaxTempC,
    cfg.triac_override_max_temp_c ?? 70,
    "0 = off, 40–120",
    undefined,
    { ...H, helpKey: "triac_override_max_temp_c" },
  );
  const uxix3Baud = numberRow("uxix3_serial_baud", T.settings.uxix3Baud, cfg.uxix3_serial_baud ?? 9600, "", undefined, {
    ...H,
    helpKey: "uxix3_serial_baud",
  });
  const pwmGpio = numberRow(
    "pwm_gpio",
    T.settings.pwmGpio,
    (cfg as RouterConfig & { pwm_gpio?: number }).pwm_gpio ?? -1,
    "4,5,14,16,17,21,25",
    undefined,
    { ...H, helpKey: "pwm_gpio" },
  );
  const pwmMode = textRow(
    "pwm_mode",
    T.settings.pwmMode,
    (cfg as RouterConfig & { pwm_mode?: string }).pwm_mode ?? "off",
    "",
    { ...H, helpKey: "pwm_mode" },
  );
  const pwmDuty = numberRow(
    "pwm_duty_percent",
    T.settings.pwmDuty,
    (cfg as RouterConfig & { pwm_duty_percent?: number }).pwm_duty_percent ?? 0,
    "0–100",
    undefined,
    { ...H, helpKey: "pwm_duty_percent" },
  );
  const pwmInvertedInput = h("input", {
    type: "checkbox",
    checked: !!(cfg as RouterConfig & { pwm_inverted?: boolean }).pwm_inverted,
  }) as HTMLInputElement;
  const pwmInvertedRow = wrapSwitchWithHelp(
    settingsSwitchLabel(pwmInvertedInput, T.settings.pwmInverted),
    "settings",
    "pwm_inverted",
  );
  const tempoRteInput = h("input", {
    type: "checkbox",
    checked: !!(cfg as RouterConfig & { tempo_rte_enabled?: boolean }).tempo_rte_enabled,
  }) as HTMLInputElement;
  const tempoRteRow = wrapSwitchWithHelp(
    settingsSwitchLabel(tempoRteInput, T.settings.tempoRteEnabled),
    "settings",
    "tempo_rte_enabled",
  );
  const tempoRteHint = h("p", { class: "field__hint" }, T.settings.tempoRteHint);
  const tempoRteStatus = buildTempoRteStatus(signal);
  tempoRteInput.addEventListener("change", () => {
    tempoRteStatus.setEnabled(tempoRteInput.checked);
  });
  tempoRteStatus.setEnabled(tempoRteInput.checked);

  const installUi = buildInstallCountrySection(
    T,
    localePref.get(),
    readInstallCountryState(cfg),
    () => {},
  );
  installUi.setFrequencyWarning(cfg.mains_frequency_warning);

  function updateNetworkDisabled() {
    const off = !!dhcpInput.checked;
    for (const f of [ipFixed.ref, gateway.ref, subnet.ref, dns.ref]) {
      f.el.disabled = off;
    }
  }
  function updateMqttDisabled() {
    const off = (parseInt(mqttPeriod.ref.el.value, 10) || 0) === 0;
    for (const f of [mqttIp.ref, mqttPort.ref, mqttUser.ref, mqttPwd.ref, mqttPrefix.ref, mqttDevice.ref]) {
      f.el.disabled = off;
    }
  }
  updateNetworkDisabled();
  updateMqttDisabled();

  const mqttTestBtn = h(
    "button",
    { type: "button", class: "btn btn--ghost" },
    T.settings.mqttTest,
  ) as HTMLButtonElement;

  async function runMqttTest() {
    if (!validateIp(mqttIp.ref)) {
      toast(T.settings.mqttTestInvalidIp, "error");
      return;
    }
    mqttTestBtn.disabled = true;
    try {
      const res = await api.mqttTest(
        {
          mqtt_ip: mqttIp.ref.read().trim(),
          mqtt_port: parseInt(mqttPort.ref.read(), 10) || 1883,
          mqtt_user: mqttUser.ref.read(),
          mqtt_password: mqttPwd.ref.read(),
          mqtt_device_name: resolveMqttDeviceName(mqttDevice.ref.read()),
        },
        { signal },
      );
      if (res.ok) {
        toast(T.settings.mqttTestOk.replace("{message}", res.message), "success");
      } else {
        toast(
          T.settings.mqttTestFailed
            .replace("{message}", res.message)
            .replace("{code}", String(res.error_code)),
          "error",
        );
      }
    } catch (e) {
      const body = e instanceof ApiError ? e.body : null;
      if (body && typeof body === "object" && "message" in body) {
        const fail = body as MqttTestResponse;
        toast(
          T.settings.mqttTestFailed
            .replace("{message}", String(fail.message))
            .replace("{code}", String(fail.error_code ?? "")),
          "error",
        );
      } else {
        toast(T.settings.actionFailed, "error");
      }
    } finally {
      mqttTestBtn.disabled = false;
    }
  }
  mqttTestBtn.addEventListener("click", () => void runMqttTest());

  const mqttActionsRow = h("div", { class: "row", style: "gap:8px;flex-wrap:wrap;margin-top:8px;" });

  async function runMqttAction(
    fn: () => Promise<unknown>,
    okMsg: string,
    btn: HTMLButtonElement,
  ) {
    btn.disabled = true;
    try {
      await fn();
      toast(okMsg, "success");
    } catch {
      toast(T.settings.actionFailed, "error");
    } finally {
      btn.disabled = false;
    }
  }

  for (const [label, fn, okMsg] of [
    [T.settings.mqttPublish, () => api.mqttPublishNow({ signal }), T.settings.mqttPublishOk],
    [T.settings.mqttDiscover, () => api.mqttRepublishDiscovery({ signal }), T.settings.mqttDiscoverOk],
    [T.settings.mqttReconnect, () => api.mqttReconnect({ signal }), T.settings.mqttReconnectOk],
  ] as const) {
    const btn = h(
      "button",
      { type: "button", class: "btn btn--ghost" },
      label,
    ) as HTMLButtonElement;
    btn.addEventListener("click", () => void runMqttAction(fn, okMsg, btn));
    mqttActionsRow.append(btn);
  }
  mqttActionsRow.prepend(mqttTestBtn);

  const timeNowEl = h("p", { class: "card__sub" }, "");
  const tzField = buildTimezoneCountryField(
    T,
    localePref.get(),
    "",
    cfg.install_country,
    () => {},
  );
  const ntp1Field = textRow("time_ntp1", T.settings.ntp1, "", "", { ...H, helpKey: "time_ntp1" });
  const ntp2Field = textRow("time_ntp2", T.settings.ntp2, "", "", { ...H, helpKey: "time_ntp2" });
  const sourceSummary = buildMeasurementSourceSummary({
    signal,
    initialConfig: cfg,
  });

  const networkLive = buildNetworkStatusSummary({
    signal,
    initialConfig: cfg,
    networkFields: {
      ipFixed: ipFixed.ref,
      gateway: gateway.ref,
      subnet: subnet.ref,
      dns: dns.ref,
    },
  });

  let dirty = false;
  const cardSavers: SettingsCardAutosave[] = [];
  const cardLabels = {
    pending: T.settings.cardPending,
    saving: T.saving,
    saved: T.saved,
    error: T.saveError,
  };
  const syncGlobalDirty = () => {
    dirty = cardSavers.some((s) => s.isDirty() || s.isPending());
  };
  const mergeCfg = (patch: Partial<RouterConfig>) => {
    cfg = { ...cfg, ...patch };
  };

  const measurementSection = buildMeasurementSettingsCard(
    T,
    cfg,
    signal,
    sourceSummary.el,
    (c) => {
      cfg = c;
      sourceSummary.setConfig(c);
      syncGlobalDirty();
    },
  );

  const advancedSection = buildAdvancedSection(T, signal, timeNowEl, tzField, ntp1Field, ntp2Field);

  const installCard = installUi.section;
  const identityCard = settingsSection(
    T.settings.sectionIdentity,
    router.el,
    probeHouse.el,
    probeSecond.el,
    probeTemp.el,
  );
  const backupCard = h(
    "section",
    { class: "card" },
    h("h2", { class: "section__title" }, T.settings.sectionBackup),
    h("p", { class: "field__hint" }, T.settings.backupHint),
    h(
      "a",
      {
        href: withBase("/backup"),
        class: "btn btn--ghost",
        "data-route": "true",
      },
      T.settings.openBackup,
    ),
  );

  const routingCard = settingsSection(
    T.settings.sectionRouting,
    h("div", { class: "field" }, vacationRow),
    vacationEnd.el,
    h("div", { class: "field" }, triacStaleRow, triacStaleHint),
    h("div", { class: "field" }, heaterBackoffRow, heaterBackoffHint),
    maxRouted.el,
    h("div", { class: "field" }, mqttJsonRow, mqttJsonHint),
  );
  const calibrationCard = settingsSection(T.settings.sectionCalibration, calibU.el, calibI.el);
  const networkCard = buildNetworkSettingsCard(
    T,
    networkLive.el,
    dhcpRow,
    ipFixed.el,
    gateway.el,
    subnet.el,
    dns.el,
  );
  const mqttCard = buildMqttSettingsCard(
    T,
    mqttPeriod.el,
    mqttIp.el,
    mqttPort.el,
    mqttUser.el,
    mqttPwd.el,
    mqttPrefix.el,
    mqttDevice.el,
    mqttActionsRow,
  );
  const advancedSourcesCard = settingsSection(
    T.settings.sectionAdvancedSources,
    pmqttTopic.el,
    ...pmqttFlow.sectionRows,
    triacTempCap.el,
    uxix3Baud.el,
    h("h3", { class: "field__hint" }, T.settings.pwmSection),
    pwmGpio.el,
    pwmMode.el,
    pwmDuty.el,
    h("div", { class: "field" }, pwmInvertedRow),
    h("div", { class: "field" }, tempoRteRow, tempoRteHint, tempoRteStatus.el),
  );

  const autosaveOpts = {
    signal,
    labels: cardLabels,
    onStateChange: syncGlobalDirty,
  };

  cardSavers.push(
    attachSettingsCardAutosave({
      ...autosaveOpts,
      card: installCard,
      collect: () => {
        const p = applyInstallCountryToConfig(cfg, installUi.getState());
        return {
          install_country: p.install_country,
          install_country_variant: p.install_country_variant ?? "",
          mains_nominal_v: p.mains_nominal_v,
          mains_frequency_mode: p.mains_frequency_mode,
          mains_frequency_hz_manual: p.mains_frequency_hz_manual,
        };
      },
      onSaved: (patch) => {
        mergeCfg(patch);
        installUi.setFrequencyWarning(cfg.mains_frequency_warning);
        networkLive.setConfig(cfg);
        if (!deviceTz) {
          vacationEnd.writeEpoch(cfg.vacation_end_epoch ?? 0);
        }
        vacationEnd.refreshTzHint();
      },
    }),
    attachSettingsCardAutosave({
      ...autosaveOpts,
      card: identityCard,
      collect: () => ({
        router_name: router.ref.read(),
        probe_house_name: probeHouse.ref.read(),
        probe_second_name: probeSecond.ref.read(),
        ...collectIdentityTemperaturePatch(tempSensorPresent, probeTemp.ref.read()),
      }),
      onSaved: mergeCfg,
    }),
    attachSettingsCardAutosave({
      ...autosaveOpts,
      card: routingCard,
      validate: () => vacationEnd.validate(),
      collect: () => ({
        vacation_enabled: vacationInput.checked,
        vacation_end_epoch: vacationEnd.readEpoch(),
        max_routed_w: parseInt(maxRouted.ref.read(), 10) || 0,
        triac_off_when_source_stale: triacStaleInput.checked,
        triac_backoff_when_heater_idle: heaterBackoffInput.checked,
        mqtt_json_commands: mqttJsonInput.checked,
      }),
      onSaved: mergeCfg,
    }),
    attachSettingsCardAutosave({
      ...autosaveOpts,
      card: calibrationCard,
      collect: () => ({
        calib_u: parseInt(calibU.ref.read(), 10) || 1000,
        calib_i: parseInt(calibI.ref.read(), 10) || 1000,
      }),
      onSaved: mergeCfg,
    }),
    attachSettingsCardAutosave({
      ...autosaveOpts,
      card: networkCard,
      validate: () => {
        if (dhcpInput.checked) return true;
        const ok = validateIp(ipFixed.ref) && validateIp(gateway.ref);
        if (!ok) toast(T.settings.badIp, "error");
        return ok;
      },
      collect: () => ({
        dhcp_on: dhcpInput.checked,
        ip_fixed: ipFixed.ref.read(),
        gateway: gateway.ref.read(),
        subnet_mask: subnet.ref.read(),
        dns: dns.ref.read(),
      }),
      onSaved: (patch) => {
        mergeCfg(patch);
        networkLive.setConfig(cfg);
      },
    }),
    attachSettingsCardAutosave({
      ...autosaveOpts,
      card: mqttCard,
      validate: () => {
        if ((parseInt(mqttPeriod.ref.read(), 10) || 0) === 0) return true;
        const ok = validateIp(mqttIp.ref);
        if (!ok) toast(T.settings.badIp, "error");
        return ok;
      },
      collect: () => ({
        mqtt_repeat_sec: parseInt(mqttPeriod.ref.read(), 10) || 0,
        mqtt_ip: mqttIp.ref.read(),
        mqtt_port: parseInt(mqttPort.ref.read(), 10) || 1883,
        mqtt_user: mqttUser.ref.read(),
        mqtt_password: mqttPwd.ref.read(),
        mqtt_prefix: mqttPrefix.ref.read(),
        mqtt_device_name: resolveMqttDeviceName(mqttDevice.ref.read()),
      }),
      onSaved: mergeCfg,
    }),
    attachSettingsCardAutosave({
      ...autosaveOpts,
      card: advancedSourcesCard,
      collect: () => {
        const g = parseInt(pwmGpio.ref.read(), 10);
        const bindings = pmqttFlow.getBindings();
        const first = bindings.find((b) => (b.enabled ?? true));
        const compatSchema =
          first?.metric === "house.snapshot"
            ? "house"
            : first?.metric === "house.signed_net_w"
              ? "Pw,Pf"
              : cfg.pmqtt_schema ?? "Pw";
        const broker = pmqttFlow.getMqttBroker();
        return {
          mqtt_ip: broker.mqtt_ip,
          mqtt_port: broker.mqtt_port,
          mqtt_user: broker.mqtt_user,
          mqtt_password: broker.mqtt_password,
          pmqtt_topic: pmqttTopic.ref.read() || first?.topic || "",
          pmqtt_schema: compatSchema,
          pmqtt_bindings: bindings,
          ...collectAdvancedTemperaturePatch(
            tempSensorPresent,
            parseInt(triacTempCap.ref.read(), 10) || 0,
          ),
          uxix3_serial_baud: parseInt(uxix3Baud.ref.read(), 10) || 9600,
          pwm_gpio: Number.isNaN(g) ? -1 : g,
          pwm_mode: pwmMode.ref.read().trim() || "off",
          pwm_duty_percent: parseInt(pwmDuty.ref.read(), 10) || 0,
          pwm_inverted: pwmInvertedInput.checked,
          tempo_rte_enabled: tempoRteInput.checked,
        };
      },
      onSaved: (patch) => {
        mergeCfg(patch);
        tempoRteStatus.setEnabled(tempoRteInput.checked);
        tempoRteStatus.refresh();
      },
    }),
    attachCardAutosave({
      ...autosaveOpts,
      card: advancedSection,
      collect: () => ({
        tz: tzField.readTz(),
        ntp1: ntp1Field.ref.read(),
        ntp2: ntp2Field.ref.read(),
      }),
      persist: async (body) => {
        await api.putTime(body, { signal, retry: 1 });
      },
      onSaved: (body) => {
        deviceTz = (body.tz || "").trim();
        vacationEnd.refreshTzHint();
      },
    }),
  );

  const sectionRoots: Record<SettingsSection, HTMLElement> = {
    general: h("div", { class: "settings-section__cards" }, installCard, identityCard, routingCard, backupCard),
    metering: h("div", { class: "settings-section__cards" }, measurementSection, calibrationCard),
    network: h("div", { class: "settings-section__cards" }, networkCard, mqttCard),
    advanced: h("div", { class: "settings-section__cards" }, advancedSourcesCard, advancedSection),
  };

  function showSection(section: SettingsSection) {
    chrome.setSection(section);
    chrome.sectionOutlet.replaceChildren(sectionRoots[section]);
  }

  showSection(initialSection);

  function applyTemperatureSettingsVisibility(present: boolean): void {
    tempSensorPresent = present;
    applyTemperatureFieldVisibility(
      { probeLabel: probeTemp.el, triacCap: triacTempCap.el },
      present,
    );
  }
  applyTemperatureSettingsVisibility(tempSensorPresent);

  const tempPoll = window.setInterval(() => {
    void api
      .getState({ signal })
      .then((st) => {
        applyTemperatureSettingsVisibility(isProbeTemperatureReading(st.temperature_c));
      })
      .catch(() => {});
  }, 30_000);

  try {
    const timeInfo = await api.getTime({ signal });
    deviceTz = (timeInfo.tz || "").trim();
    tzField.writeTz(deviceTz);
    ntp1Field.ref.write(timeInfo.ntp1 || "");
    ntp2Field.ref.write(timeInfo.ntp2 || "");
    vacationEnd.writeEpoch(cfg.vacation_end_epoch ?? 0);
    vacationEnd.refreshTzHint();
    timeNowEl.textContent = timeInfo.date_valid
      ? `${T.settings.timeNow}: ${timeInfo.now}`
      : T.settings.timeInvalid;
  } catch {
    timeNowEl.textContent = T.settings.timeInvalid;
  }

  function beforeUnload(ev: BeforeUnloadEvent) {
    if (!dirty) return;
    ev.preventDefault();
    ev.returnValue = "";
  }
  window.addEventListener("beforeunload", beforeUnload);
  setUnsavedGuard(() => dirty);
  signal.addEventListener(
    "abort",
    () => {
      window.removeEventListener("beforeunload", beforeUnload);
      setUnsavedGuard(null);
    },
    { once: true },
  );

  const cleanup = () => {
    void Promise.all(cardSavers.map((s) => s.flush())).finally(() => {
      window.clearInterval(tempPoll);
      sourceSummary.stop();
      networkLive.stop();
      tempoRteStatus.stop();
      window.removeEventListener("beforeunload", beforeUnload);
      setUnsavedGuard(null);
      setSettingsLayoutHandlers(null, null);
    });
  };

  setSettingsLayoutHandlers(
    (path) => showSection(parseSettingsSection(path)),
    cleanup,
  );

  return cleanup;
}
