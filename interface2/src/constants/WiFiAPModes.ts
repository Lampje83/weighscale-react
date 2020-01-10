import { APSettingsData } from "../containers/APSettings";

export const WIFI_AP_MODE_ALWAYS = 0;
export const WIFI_AP_MODE_DISCONNECTED = 1;
export const WIFI_AP_NEVER = 2;

export const isAPEnabled = ({ provision_mode }: APSettingsData) => provision_mode === WIFI_AP_MODE_ALWAYS || provision_mode === WIFI_AP_MODE_DISCONNECTED;