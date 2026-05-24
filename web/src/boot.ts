import { ensureUiFresh } from "./pwa/uiCacheRefresh";
import { installPwaManifest } from "./pwa/installManifest";
import { registerServiceWorker } from "./pwa/registerServiceWorker";

void (async () => {
  installPwaManifest();
  await registerServiceWorker();
  await ensureUiFresh();
  await import("./main");
})();
