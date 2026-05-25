import { describe, expect, it, vi, beforeEach } from "vitest";
import { mountApi } from "../src/routes/Api";
import { en } from "../src/i18n/locales/en";
import { publicBootstrap } from "../src/state/store";
import { clearSession } from "../src/api/apiSession";

const getConfig = vi.hoisted(() =>
  vi.fn(async () => ({ config: { http_cors_enabled: false } })),
);
const getPublic = vi.hoisted(() => vi.fn());
const listAuthTokens = vi.hoisted(() => vi.fn());

vi.mock("../src/api/client", async (importOriginal) => {
  const orig = await importOriginal<typeof import("../src/api/client")>();
  return {
    ...orig,
    api: {
      ...orig.api,
      getConfig,
      getPublic,
      listAuthTokens,
    },
  };
});

const samplePublic = (enabled: boolean) => ({
  http_auth: { enabled, username: "admin" },
  device: {
    router_name: "HelioZero",
    firmware_version: "0.1.0",
    source_configured: true,
  },
  wifi: { mode: "sta" as const, connected: true },
});

describe("Api page permanent tokens", () => {
  beforeEach(() => {
    document.body.innerHTML = "";
    clearSession();
    publicBootstrap.set({ ready: false, httpAuthEnabled: false });
    getConfig.mockClear();
    listAuthTokens.mockReset();
  });

  it("disables create token when HTTP API password is not enabled", async () => {
    getPublic.mockResolvedValue(samplePublic(false));
    const outlet = document.createElement("div");
    document.body.append(outlet);
    const cleanup = await mountApi({ outlet, signal: new AbortController().signal });
    await new Promise((r) => setTimeout(r, 0));

    const tokensCard = Array.from(outlet.querySelectorAll(".card")).find((c) =>
      c.textContent?.includes(en.apiPage.sectionTokens),
    );
    const btn = tokensCard?.querySelector(".btn--primary") as HTMLButtonElement;
    expect(btn?.textContent).toBe(en.apiPage.tokenCreate);
    expect(btn?.disabled).toBe(true);
    expect(
      tokensCard?.querySelector(".banner.banner--warn[role='alert']")?.hidden,
    ).toBe(false);
    cleanup();
  });

  it("disables create token when auth enabled but not signed in", async () => {
    getPublic.mockResolvedValue(samplePublic(true));
    listAuthTokens.mockResolvedValue([]);
    publicBootstrap.set({ ready: true, httpAuthEnabled: true });
    const outlet = document.createElement("div");
    document.body.append(outlet);
    const cleanup = await mountApi({ outlet, signal: new AbortController().signal });
    await new Promise((r) => setTimeout(r, 0));

    const tokensCard = Array.from(outlet.querySelectorAll(".card")).find((c) =>
      c.textContent?.includes(en.apiPage.sectionTokens),
    );
    const btn = tokensCard?.querySelector(".btn--primary") as HTMLButtonElement;
    expect(btn?.disabled).toBe(true);
    expect(tokensCard?.querySelector(".banner.banner--info")?.hidden).toBe(false);
    cleanup();
  });
});
