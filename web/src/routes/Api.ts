import type { RouteCtx } from "../router";
import { h } from "../utils/dom";
import { api, ApiError } from "../api/client";
import type { AuthTokenInfo } from "../api/types";
import { toast } from "../components/Toast";
import { openDialog } from "../components/Dialog";
import { getStrings } from "../i18n";
import { localePref } from "../state/store";
import { wrapSwitchWithHelp } from "../components/FieldHelp";
import { settingsSwitchLabel } from "../utils/settingsSwitch";
import { attachSettingsCardAutosave } from "./settings/cardAutosave";
import { buildHttpAuthSection } from "./api/httpAuthSection";
import { buildPageHeader } from "../components/ui/pageHeader";
import { docsPageUrl } from "../fieldHelp/docUrl";

export async function mountApi(ctx: RouteCtx): Promise<() => void> {
  const { outlet, signal } = ctx;
  const T = getStrings();
  const A = T.apiPage;

  outlet.append(buildPageHeader({ title: A.title, description: A.docsHint }));

  const loading = h("p", { class: "empty" }, T.loading);
  outlet.append(loading);

  let httpCorsEnabled = false;
  try {
    const { config } = await api.getConfig({ signal });
    httpCorsEnabled = !!config.http_cors_enabled;
  } catch (e) {
    if ((e as DOMException)?.name === "AbortError") return () => {};
    loading.textContent = T.status.error + " — " + T.retry;
    return () => {};
  }
  loading.remove();

  const docLang = localePref.get() === "fr" ? "fr" : "en";
  const docsCard = h(
    "section",
    { class: "card" },
    h("h2", { class: "section__title" }, A.sectionDocs),
    h("p", { class: "field__hint" }, A.docsHint),
    h(
      "p",
      {},
      h(
        "a",
        {
          href: docsPageUrl(`/${docLang}/api/`),
          target: "_blank",
          rel: "noopener",
        },
        A.docsOpenApi,
      ),
      " · ",
      h(
        "a",
        {
          href: docsPageUrl(`/${docLang}/http-api-security/`),
          target: "_blank",
          rel: "noopener",
        },
        A.docsSecurity,
      ),
    ),
  );

  const httpCorsInput = h("input", {
    type: "checkbox",
    checked: httpCorsEnabled,
  }) as HTMLInputElement;
  const corsCard = h(
    "section",
    { class: "card" },
    h("h2", { class: "section__title" }, A.sectionCors),
    h(
      "div",
      { class: "field" },
      wrapSwitchWithHelp(
        settingsSwitchLabel(httpCorsInput, A.httpCorsEnabled),
        "api",
        "http_cors_enabled",
      ),
      h("p", { class: "field__hint" }, A.httpCorsHint),
    ),
  );

  const tokensList = h("ul", { class: "api-token-list" });
  const tokensEmpty = h("p", { class: "field__hint" }, A.tokensEmpty);
  const tokensDisabled = h("p", { class: "field__hint", hidden: true }, A.tokensNeedAuth);

  async function refreshTokens() {
    tokensList.replaceChildren();
    try {
      const pub = await api.getPublic({ signal });
      if (!pub.http_auth.enabled) {
        tokensDisabled.hidden = false;
        tokensEmpty.hidden = true;
        return;
      }
      tokensDisabled.hidden = true;
      const items = await api.listAuthTokens({ signal });
      if (items.length === 0) {
        tokensEmpty.hidden = false;
        return;
      }
      tokensEmpty.hidden = true;
      for (const t of items) {
        tokensList.append(
          h(
            "li",
            { class: "api-token-list__item spread" },
            h("span", {}, `${t.label} (id ${t.id})`),
            h(
              "button",
              {
                type: "button",
                class: "btn btn--ghost btn--sm",
                onClick: () => void revokeToken(t),
              },
              A.tokenRevoke,
            ),
          ),
        );
      }
    } catch {
      tokensEmpty.hidden = false;
      tokensEmpty.textContent = T.status.error;
    }
  }

  function showTokenOnce(token: string, label: string) {
    const input = h("input", {
      type: "text",
      class: "field__input",
      readonly: true,
      value: token,
    }) as HTMLInputElement;
    openDialog({
      title: A.tokenCreatedTitle,
      body: h(
        "div",
        {},
        h("p", {}, A.tokenCreatedBody.replace("{label}", label)),
        input,
      ),
      actions: [
        {
          label: A.tokenCopy,
          kind: "primary",
          onClick: async () => {
            try {
              await navigator.clipboard.writeText(token);
              toast(T.copyOk, "success");
            } catch {
              input.select();
              toast(T.copyErr, "error");
            }
          },
        },
        { label: T.close, kind: "ghost", onClick: () => {} },
      ],
    });
  }

  async function revokeToken(t: AuthTokenInfo) {
    openDialog({
      title: A.tokenRevokeConfirmTitle,
      body: h("p", {}, A.tokenRevokeConfirmBody.replace("{label}", t.label)),
      actions: [
        { label: T.cancel, kind: "ghost", onClick: () => {} },
        {
          label: A.tokenRevoke,
          kind: "danger",
          onClick: async () => {
            try {
              await api.revokeAuthToken(t.id, { signal });
              toast(T.saved, "success");
              await refreshTokens();
            } catch (e) {
              toast(e instanceof ApiError ? e.message : T.saveError, "error");
            }
          },
        },
      ],
    });
  }

  const labelInput = h("input", {
    type: "text",
    class: "field__input",
    placeholder: A.tokenLabelPlaceholder,
    maxlength: "24",
  }) as HTMLInputElement;

  const tokensCard = h(
    "section",
    { class: "card" },
    h("h2", { class: "section__title" }, A.sectionTokens),
    h("p", { class: "field__hint" }, A.tokensHint),
    tokensDisabled,
    tokensEmpty,
    tokensList,
    h("div", { class: "field" }, h("label", { class: "field__label" }, A.tokenLabel), labelInput),
    h(
      "button",
      {
        type: "button",
        class: "btn btn--primary",
        style: "margin-top:8px;",
        onClick: async () => {
          try {
            const res = await api.createAuthToken(
              labelInput.value.trim() || undefined,
              { signal },
            );
            labelInput.value = "";
            showTokenOnce(res.token, res.label);
            await refreshTokens();
          } catch (e) {
            toast(e instanceof ApiError ? e.message : T.saveError, "error");
          }
        },
      },
      A.tokenCreate,
    ),
  );

  const httpAuthSection = await buildHttpAuthSection(T, signal, { helpScope: "api" });
  await refreshTokens();

  const form = h("form", { class: "form", onSubmit: (e) => e.preventDefault() });
  form.append(docsCard, httpAuthSection, tokensCard, corsCard);
  outlet.append(form);

  const cardLabels = {
    pending: T.settings.cardPending,
    saving: T.saving,
    saved: T.saved,
    error: T.saveError,
  };

  const cardSavers = [
    attachSettingsCardAutosave({
      card: corsCard,
      signal,
      watchRoots: [httpCorsInput],
      collect: () => ({ http_cors_enabled: httpCorsInput.checked }),
      onSaved: (patch) => {
        if (typeof patch.http_cors_enabled === "boolean") {
          httpCorsEnabled = patch.http_cors_enabled;
        }
      },
      labels: cardLabels,
    }),
  ];

  return () => {
    void Promise.all(cardSavers.map((s) => s.flush()));
  };
}
