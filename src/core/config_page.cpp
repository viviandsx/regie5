/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The page a phone opens to fill config.json (core/config_web.h): one file,
 * no outside resource (the console's network may not reach the internet).
 * The code shown on the TV comes in the address (?c=...) and goes with every
 * request.
 */
#include "core/config_web.h"

namespace config_web {

namespace {

const char *kPage = R"PAGE(<!doctype html>
<html lang="__LANG__"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Régie5</title>
<style>
:root{--bg:#0b0c11;--card:#171821;--line:#2a2c38;--text:#f4f3ef;--dim:#a3a6b4;--accent:#ffb240;--ok:#2ed6be;--bad:#ff6a48}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;padding:16px}
h1{font-size:24px;margin:4px 0 4px}h1 b{color:var(--accent)}
p.lead{color:var(--dim);margin:0 0 16px}
section{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin:0 0 14px}
h2{font-size:18px;margin:0 0 8px;display:flex;justify-content:space-between;align-items:center}
.inst{border-top:1px solid var(--line);padding-top:10px;margin-top:10px}
.inst:first-of-type{border-top:0;margin-top:0;padding-top:0}
label{display:block;font-size:13px;color:var(--dim);margin:8px 0 3px}
input[type=text],input[type=url],input[type=password]{width:100%;padding:11px 12px;border-radius:10px;border:1px solid var(--line);background:#0f1016;color:var(--text);font-size:16px}
.row{display:flex;gap:16px;align-items:center;margin-top:8px;flex-wrap:wrap}
.row label{display:flex;gap:6px;align-items:center;margin:0;color:var(--text);font-size:15px}
button{font-size:16px;border-radius:10px;border:1px solid var(--line);background:#232532;color:var(--text);padding:10px 14px}
button.main{width:100%;background:var(--accent);color:#1a1206;border:0;font-weight:600;padding:14px}
button.small{padding:6px 10px;font-size:14px}
.hint{font-size:13px;color:var(--dim)}
#msg{margin:12px 0;padding:12px;border-radius:10px;display:none}
#msg.ok{display:block;background:#123a35;color:#c9fff6}#msg.bad{display:block;background:#3d1a14;color:#ffd9cf}
</style></head><body>
<h1>Régie<b>5</b></h1>
<p class="lead" id="lead"></p>
<div id="msg"></div>
<form id="f"></form>
<script>
const FR = __FR__;
const T = FR ? {
 lead:"Adresses et clés d'API de vos services. Les clés déjà enregistrées ne sont jamais affichées : laissez le champ vide pour les garder.",
 name:"Nom (facultatif)", url:"Adresse", key:"Clé d'API", keyKept:"Clé enregistrée — vide pour la garder",
 user:"Utilisateur (facultatif)", pass:"Mot de passe (facultatif)", passKept:"Mot de passe enregistré — vide pour le garder",
 on:"Activé", k4:"Instance 4K", insecure:"Certificat auto-signé (https, non vérifié)", add:"+ Instance", remove:"Retirer", save:"Enregistrer sur la console",
 saving:"Enregistrement…", saved:"Enregistré. Régie5 teste les services : regardez la TV.", failed:"Refusé : ",
 lost:"La console ne répond plus (fenêtre fermée sur la TV ?).", badcode:"Code refusé : rouvrez la page depuis le QR code affiché sur la TV.",
 optional:"facultatif", askCode:"Code à 6 chiffres affiché sur la TV :"
} : {
 lead:"Addresses and API keys of your services. Saved keys are never shown: leave the field empty to keep them.",
 name:"Name (optional)", url:"Address", key:"API key", keyKept:"Key saved — leave empty to keep it",
 user:"User (optional)", pass:"Password (optional)", passKept:"Password saved — leave empty to keep it",
 on:"On", k4:"4K instance", insecure:"Self-signed certificate (https, not checked)", add:"+ Instance", remove:"Remove", save:"Save on the console",
 saving:"Saving…", saved:"Saved. Régie5 is testing the services: look at the TV.", failed:"Refused: ",
 lost:"The console does not answer (window closed on the TV?).", badcode:"Code refused: open the page again from the QR code on the TV.",
 optional:"optional", askCode:"6-digit code shown on the TV:"
};
const SERVICES = [["sonarr","Sonarr",true],["radarr","Radarr",true],["seerr","Seerr",false],["jellyfin","Jellyfin",false],["glances","Glances",false]];
const PORTS = {sonarr: 8989, radarr: 7878, seerr: 5055, jellyfin: 8096, glances: 61208};
const code = new URLSearchParams(location.search).get("c") || (prompt(T.askCode) || "").replace(/\D/g, "");
const api = "/api/config?c=" + encodeURIComponent(code);
document.getElementById("lead").textContent = T.lead;
const form = document.getElementById("f");
const msg = document.getElementById("msg");
function say(kind, text){ msg.className = kind; msg.textContent = text; msg.scrollIntoView({behavior: "smooth", block: "center"}); }
function el(tag, props, kids){ const e = document.createElement(tag); Object.assign(e, props || {}); (kids || []).forEach(k => e.appendChild(k)); return e; }
function field(label, input){ return el("div", {}, [el("label", {textContent: label}), input]); }
function instance(key, multi, data, from){
  const g = key === "glances";
  const box = el("div", {className: "inst"});
  box.dataset.from = from;
  const name = el("input", {type: "text", value: data.name || "", className: "name"});
  const url = el("input", {type: "url", value: data.url || "", className: "url", placeholder: "http://192.168.1.10:" + PORTS[key], autocapitalize: "off", autocomplete: "off"});
  box.appendChild(field(T.name, name));
  box.appendChild(field(T.url, url));
  if (g) {
    box.appendChild(field(T.user, el("input", {type: "text", value: data.username || "", className: "user", autocapitalize: "off"})));
    box.appendChild(field(T.pass, el("input", {type: "password", className: "secret", placeholder: data.hasPassword ? T.passKept : T.optional})));
  } else {
    box.appendChild(field(T.key, el("input", {type: "text", className: "secret", autocapitalize: "off", autocomplete: "off", spellcheck: false, placeholder: data.hasKey ? T.keyKept : ""})));
  }
  const row = el("div", {className: "row"});
  const on = el("input", {type: "checkbox", checked: data.enabled !== false, className: "on"});
  row.appendChild(el("label", {}, [on, document.createTextNode(T.on)]));
  if (multi) {
    const k4 = el("input", {type: "checkbox", checked: !!data.is4k, className: "k4"});
    row.appendChild(el("label", {}, [k4, document.createTextNode(T.k4)]));
    const rm = el("button", {type: "button", className: "small", textContent: T.remove});
    rm.onclick = () => box.remove();
    row.appendChild(rm);
  }
  box.appendChild(row);
  const sec = el("input", {type: "checkbox", checked: !!data.insecure, className: "insecure"});
  const secRow = el("div", {className: "row"}, [el("label", {}, [sec, document.createTextNode(T.insecure)])]);
  const showSec = () => secRow.style.display = url.value.trim().toLowerCase().startsWith("https://") ? "" : "none";
  url.addEventListener("input", showSec);
  showSec();
  box.appendChild(secRow);
  return box;
}
function build(view){
  form.innerHTML = "";
  for (const [key, title, multi] of SERVICES) {
    const list = (view.services && view.services[key]) || [];
    const sec = el("section", {});
    sec.dataset.key = key;
    const head = el("h2", {textContent: title});
    const holder = el("div", {});
    if (multi) {
      const add = el("button", {type: "button", className: "small", textContent: T.add});
      add.onclick = () => holder.appendChild(instance(key, true, {enabled: true}, -1));
      head.appendChild(add);
    }
    sec.appendChild(head);
    (list.length ? list : [{enabled: key !== "glances"}]).forEach((d, i) => holder.appendChild(instance(key, multi, d, list.length ? i : -1)));
    sec.appendChild(holder);
    form.appendChild(sec);
  }
  const save = el("button", {type: "submit", className: "main", textContent: T.save});
  form.appendChild(save);
}
function collect(){
  const services = {};
  form.querySelectorAll("section").forEach(sec => {
    const key = sec.dataset.key;
    services[key] = [...sec.querySelectorAll(".inst")].map(b => {
      const o = {from: Number(b.dataset.from), name: b.querySelector(".name").value.trim(),
                 url: b.querySelector(".url").value.trim(), enabled: b.querySelector(".on").checked,
                 insecure: b.querySelector(".url").value.trim().toLowerCase().startsWith("https://") && b.querySelector(".insecure").checked};
      const secret = b.querySelector(".secret").value.trim();
      if (key === "glances") { o.username = b.querySelector(".user").value.trim(); o.password = secret; }
      else o.apiKey = secret;
      const k4 = b.querySelector(".k4"); if (k4) o.is4k = k4.checked;
      return o;
    });
  });
  return {services};
}
form.onsubmit = async (e) => {
  e.preventDefault();
  say("ok", T.saving);
  try {
    const r = await fetch(api, {method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify(collect())});
    const a = await r.json().catch(() => ({}));
    if (r.status === 403) return say("bad", T.badcode);
    if (r.ok && a.ok) { say("ok", T.saved); form.querySelectorAll(".secret").forEach(i => i.value = ""); load(); }
    else say("bad", T.failed + (a.error || r.status));
  } catch (err) { say("bad", T.lost); }
};
async function load(){
  try {
    const r = await fetch(api);
    if (r.status === 403) return say("bad", T.badcode);
    build(await r.json());
  } catch (err) { say("bad", T.lost); }
}
load();
</script></body></html>
)PAGE";

void replace(std::string &s, const std::string &from, const std::string &to)
{
    const size_t at = s.find(from);
    if (at != std::string::npos)
        s.replace(at, from.size(), to);
}

} // namespace

std::string page_html(bool french)
{
    std::string page = kPage;
    replace(page, "__LANG__", french ? "fr" : "en");
    replace(page, "__FR__", french ? "true" : "false");
    return page;
}

} // namespace config_web
