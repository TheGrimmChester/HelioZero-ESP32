import { pinoutSectionUrl as pinoutDocUrl } from "../../fieldHelp/docUrl";

/** Wire IDs from firmware `kRegistry` (helio_source.cpp), excluding NotDef as a target. */
export type SourceWireId =
  | "UxIx2"
  | "UxIx3"
  | "UxI"
  | "Linky"
  | "Enphase"
  | "ShellyEm"
  | "ShellyPro"
  | "SmartG"
  | "HomeW"
  | "Pmqtt"
  | "Ext";

export interface SourceRegistryEntry {
  id: SourceWireId;
  /** /en/hardware-pinout/ §17 subsection anchor. */
  pinoutAnchor: string;
  fields: (
    | "ext_peer_ip"
    | "ext_peer_port"
    | "ext_peer_path"
    | "ext_protocol"
    | "enphase_user"
    | "enphase_password"
    | "enphase_serial"
    | "meter_channel"
    | "pmqtt_topic"
    | "pmqtt_schema"
    | "uxix3_serial_baud"
    | "calib_u"
    | "calib_i"
  )[];
}

const LAN_HTTP = "17-sources-de-mesure--schémas-électrique-et-électronique";

export const SOURCE_REGISTRY: SourceRegistryEntry[] = [
  { id: "UxIx2", pinoutAnchor: "source_uxix2", fields: [] },
  { id: "UxIx3", pinoutAnchor: "source_uxix3", fields: ["uxix3_serial_baud"] },
  { id: "UxI", pinoutAnchor: "source_uxi", fields: ["calib_u", "calib_i"] },
  { id: "Linky", pinoutAnchor: "source_linky", fields: [] },
  { id: "Enphase", pinoutAnchor: "source_enphase", fields: ["enphase_user", "enphase_password", "enphase_serial"] },
  { id: "ShellyEm", pinoutAnchor: "source_shellyem", fields: ["ext_peer_ip"] },
  {
    id: "ShellyPro",
    pinoutAnchor: LAN_HTTP,
    fields: ["ext_peer_ip", "meter_channel"],
  },
  { id: "SmartG", pinoutAnchor: "source_smartg", fields: ["ext_peer_ip"] },
  { id: "HomeW", pinoutAnchor: LAN_HTTP, fields: ["ext_peer_ip"] },
  { id: "Pmqtt", pinoutAnchor: LAN_HTTP, fields: ["pmqtt_topic", "pmqtt_schema"] },
  {
    id: "Ext",
    pinoutAnchor: "source_ext",
    fields: ["ext_peer_ip", "ext_peer_port", "ext_peer_path"],
  },
];

export function pinoutSectionUrl(anchor: string): string {
  return pinoutDocUrl(anchor);
}

export function registryEntry(id: SourceWireId): SourceRegistryEntry | undefined {
  return SOURCE_REGISTRY.find((e) => e.id === id);
}
