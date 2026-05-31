import { pinoutSectionUrl as pinoutDocUrl } from "../../fieldHelp/docUrl";

/** Wire IDs from firmware `kRegistry` (helio_source.cpp), excluding NotDef as a target. */
export type SourceWireId =
  | "JsyMk194"
  | "JsyMk333"
  | "Analog"
  | "Linky"
  | "Enphase"
  | "ShellyEm"
  | "ShellyPro"
  | "SmartG"
  | "HomeW"
  | "Pmqtt"
  | "HelioPeer";

export interface SourceRegistryEntry {
  id: SourceWireId;
  /** /en/hardware-pinout/ §17 subsection anchor. */
  pinoutAnchor: string;
  fields: (
    | "peer_ip"
    | "peer_port"
    | "peer_path"
    | "peer_protocol"
    | "enphase_user"
    | "enphase_password"
    | "enphase_serial"
    | "meter_channel"
    | "pmqtt_topic"
    | "pmqtt_schema"
    | "jsy_mk333_serial_baud"
    | "calib_u"
    | "calib_i"
  )[];
}

const LAN_HTTP = "17-sources-de-mesure--schémas-électrique-et-électronique";

export const SOURCE_REGISTRY: SourceRegistryEntry[] = [
  { id: "JsyMk194", pinoutAnchor: "source_jsy_mk194", fields: [] },
  { id: "JsyMk333", pinoutAnchor: "source_jsy_mk333", fields: ["jsy_mk333_serial_baud"] },
  { id: "Analog", pinoutAnchor: "source_analog", fields: ["calib_u", "calib_i"] },
  { id: "Linky", pinoutAnchor: "source_linky", fields: [] },
  { id: "Enphase", pinoutAnchor: "source_enphase", fields: ["enphase_user", "enphase_password", "enphase_serial"] },
  { id: "ShellyEm", pinoutAnchor: "source_shellyem", fields: ["peer_ip"] },
  {
    id: "ShellyPro",
    pinoutAnchor: LAN_HTTP,
    fields: ["peer_ip", "meter_channel"],
  },
  { id: "SmartG", pinoutAnchor: "source_smartg", fields: ["peer_ip"] },
  { id: "HomeW", pinoutAnchor: LAN_HTTP, fields: ["peer_ip"] },
  { id: "Pmqtt", pinoutAnchor: LAN_HTTP, fields: ["pmqtt_topic", "pmqtt_schema"] },
  {
    id: "HelioPeer",
    pinoutAnchor: "source_helio_peer",
    fields: ["peer_ip", "peer_port", "peer_path"],
  },
];

export function pinoutSectionUrl(anchor: string): string {
  return pinoutDocUrl(anchor);
}

export function registryEntry(id: SourceWireId): SourceRegistryEntry | undefined {
  return SOURCE_REGISTRY.find((e) => e.id === id);
}
