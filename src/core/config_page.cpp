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

#include "core/i18n.h"

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
const T = __T__;
const SERVICES = [["sonarr","Sonarr",true],["radarr","Radarr",true],["seerr","Seerr",false],["jellyfin","Jellyfin",false],["emby","Emby",false],["plex","Plex",false],["glances","Glances",false]];
const PORTS = {sonarr: 8989, radarr: 7878, seerr: 5055, jellyfin: 8096, emby: 8096, plex: 32400, glances: 61208};
const MEDIA = ["jellyfin", "emby", "plex"];
const query = new URLSearchParams(location.search);
const code = query.get("c") || (prompt(T.askCode) || "").replace(/\D/g, "");
/* m: the media server chosen in the welcome screens (jellyfin, emby, plex,
 * none): the others' sections are not shown, and so not sent (left as they are). */
const media = query.get("m");
const shown = key => !media || !MEDIA.includes(key) || key === media;
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
  } else if (key === "plex") {   /* signed in from here (plex.tv, the phone's internet); a pasted token: the fallback */
    if (data.userName) box.appendChild(el("p", {className: "hint", textContent: data.userName + (data.owner === false ? " · " + T.plexNotOwner : "")}));
    const status = el("p", {className: "hint"});
    const signIn = el("button", {type: "button", textContent: T.plexSignIn});
    signIn.onclick = () => plexSignIn(box, url, status, signIn);
    box.appendChild(el("div", {className: "row"}, [signIn]));
    box.appendChild(el("p", {className: "hint", textContent: T.plexOwnerWarn}));
    box.appendChild(status);
    const tok = el("input", {type: "text", className: "secret", autocapitalize: "off", autocomplete: "off", spellcheck: false, placeholder: data.hasKey ? T.keyKept : ""});
    box.appendChild(field(T.plexToken, tok));
    box.appendChild(el("p", {className: "hint", textContent: T.plexTokenHint}));
    const warn = el("p", {className: "hint", textContent: T.noTokenWarn});
    const showWarn = () => warn.style.display = !data.hasKey && !tok.value.trim() ? "" : "none";
    tok.addEventListener("input", showWarn);
    showWarn();
    box.appendChild(warn);
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
/* Plex: the PIN signs in on plex.tv, from this phone (it reaches the
 * internet; the console may not). The console gets the token Plex gives for
 * the server: for its owner, the account's own (docs/PLEX.md § 8), which the
 * page says before signing in. */
function plexClientId(){
  let id = "";
  try { id = localStorage.getItem("regie5-plex-client") || ""; } catch (e) {}
  if (!id) {
    id = "regie5-" + Array.from(crypto.getRandomValues(new Uint8Array(12)), b => b.toString(16).padStart(2, "0")).join("");
    try { localStorage.setItem("regie5-plex-client", id); } catch (e) {}
  }
  return id;
}
function plexHeaders(token){
  const h = {"Accept": "application/json", "X-Plex-Product": "Regie5", "X-Plex-Client-Identifier": plexClientId()};
  if (token) h["X-Plex-Token"] = token;
  return h;
}
const wait = ms => new Promise(r => setTimeout(r, ms));
/* The saved address among a server's ways in (host and port). */
function sameServer(address, server){
  let u;
  try { u = new URL(address); } catch (e) { return false; }
  const port = u.port || (u.protocol === "https:" ? "443" : "80");
  return (server.connections || []).some(c => {
    try {
      const v = new URL(c.uri);
      return (v.hostname === u.hostname || c.address === u.hostname) && String(c.port || v.port) === port;
    } catch (e) { return c.address === u.hostname && String(c.port) === port; }
  });
}
async function plexSignIn(box, urlInput, status, button){
  const address = urlInput.value.trim();
  if (!/^https?:\/\//i.test(address)) { status.textContent = T.plexNoUrl; return; }
  const tab = window.open("about:blank", "_blank");   /* opened on the tap itself: no blocker in the way */
  button.disabled = true;
  try {
    let r = await fetch("https://plex.tv/api/v2/pins?strong=true", {method: "POST", headers: plexHeaders()});
    if (!r.ok) throw new Error("plex.tv " + r.status);
    const pin = await r.json();
    const auth = "https://app.plex.tv/auth#?clientID=" + encodeURIComponent(plexClientId()) + "&code=" +
                 encodeURIComponent(pin.code) + "&context%5Bdevice%5D%5Bproduct%5D=Regie5";
    if (tab) tab.location.href = auth; else window.open(auth, "_blank");
    status.textContent = T.plexWaiting;
    let token = "";
    for (let i = 0; i < 600 && !token; i++) {   /* once a second, ten minutes at most */
      await wait(1000);
      r = await fetch("https://plex.tv/api/v2/pins/" + encodeURIComponent(pin.id), {headers: plexHeaders()});
      if (r.status === 404) break;   /* expired */
      if (r.ok) token = (await r.json()).authToken || "";
    }
    if (!token) { status.textContent = T.plexExpired; return; }
    const who = await (await fetch("https://plex.tv/api/v2/user", {headers: plexHeaders(token)})).json();
    r = await fetch("https://clients.plex.tv/api/v2/resources?includeHttps=1", {headers: plexHeaders(token)});
    if (!r.ok) throw new Error("plex.tv " + r.status);
    const servers = (await r.json()).filter(d => String(d.provides || "").split(",").includes("server") && d.accessToken);
    if (!servers.length) { status.textContent = T.plexNoServer; return; }
    let server = servers.find(s => sameServer(address, s)) || (servers.length === 1 ? servers[0] : null);
    if (!server) server = await plexChoose(box, servers, status);
    status.textContent = T.saving;
    const o = {from: Number(box.dataset.from), name: box.querySelector(".name").value.trim(), url: address,
               enabled: box.querySelector(".on").checked,
               insecure: address.toLowerCase().startsWith("https://") && box.querySelector(".insecure").checked,
               apiKey: server.accessToken, userName: who.username || who.title || "", owner: !!server.owned};
    const a = await fetch(api, {method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify({services: {plex: [o]}})});
    const answer = await a.json().catch(() => ({}));
    if (a.ok && answer.ok) { say("ok", T.plexDone); load(); }
    else status.textContent = T.failed + (answer.error || a.status);
  } catch (err) {
    if (tab && !tab.closed && tab.location.href === "about:blank") tab.close();
    status.textContent = T.plexFailed + (err && err.message ? err.message : "");
  } finally {
    button.disabled = false;
  }
}
/* Several servers, none at the address: the user picks. */
function plexChoose(box, servers, status){
  status.textContent = T.plexChoose;
  return new Promise(resolve => {
    const list = el("div", {className: "row"});
    servers.forEach(s => {
      const b = el("button", {type: "button", textContent: s.name + (s.owned ? "" : " · " + T.plexNotOwner)});
      b.onclick = () => { list.remove(); resolve(s); };
      list.appendChild(b);
    });
    status.after(list);
  });
}
function build(view){
  form.innerHTML = "";
  for (const [key, title, multi] of SERVICES.filter(s => shown(s[0]))) {
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
    if (key === "emby" && !media) sec.appendChild(el("p", {className: "lead", textContent: T.embyNote}));
    if (key === "plex" && !media) sec.appendChild(el("p", {className: "lead", textContent: T.plexNote}));
    (list.length ? list : [{enabled: key !== "glances" && (!["emby", "plex"].includes(key) || media === key)}]).forEach((d, i) => holder.appendChild(instance(key, multi, d, list.length ? i : -1)));
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

/* A text as a JavaScript string. */
static std::string js(const char *s)
{
    std::string out = "\"";
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\')
            out += '\\';
        if (*p == '<')
            out += "\\u003c";   /* never a closing tag inside the script */
        else if (*p == '\n')
            out += "\\n";
        else
            out += *p;
    }
    return out + "\"";
}

std::string page_html()
{
    static const struct {
        const char *name;
        i18n::S key;
    } kTexts[] = {
        {"lead", i18n::PhoneLead},
        {"name", i18n::PhoneName},
        {"url", i18n::PhoneUrl},
        {"key", i18n::PhoneKey},
        {"keyKept", i18n::PhoneKeyKept},
        {"user", i18n::PhoneUser},
        {"pass", i18n::PhonePass},
        {"passKept", i18n::PhonePassKept},
        {"on", i18n::PhoneOn},
        {"k4", i18n::PhoneK4},
        {"insecure", i18n::PhoneInsecure},
        {"add", i18n::PhoneAdd},
        {"remove", i18n::PhoneRemove},
        {"save", i18n::PhoneSave},
        {"saving", i18n::PhoneSaving},
        {"saved", i18n::PhoneSaved},
        {"failed", i18n::PhoneFailed},
        {"lost", i18n::PhoneLost},
        {"badcode", i18n::PhoneBadcode},
        {"optional", i18n::PhoneOptional},
        {"askCode", i18n::PhoneAskCode},
        {"embyNote", i18n::PhoneEmbyNote},
        {"plexNote", i18n::PhonePlexNote},
        {"plexToken", i18n::PhonePlexToken},
        {"plexTokenHint", i18n::PhonePlexTokenHint},
        {"plexNotOwner", i18n::PlexNotOwner},
        {"noTokenWarn", i18n::PhoneNoTokenWarn},
        {"plexSignIn", i18n::PhonePlexSignIn},
        {"plexWaiting", i18n::PhonePlexWaiting},
        {"plexNoUrl", i18n::PhonePlexNoUrl},
        {"plexExpired", i18n::PhonePlexExpired},
        {"plexNoServer", i18n::PhonePlexNoServer},
        {"plexChoose", i18n::PhonePlexChoose},
        {"plexFailed", i18n::PhonePlexFailed},
        {"plexDone", i18n::PhonePlexDone},
        {"plexOwnerWarn", i18n::PhonePlexOwnerWarn},
    };
    std::string t = "{";
    for (const auto &x : kTexts)
        t += std::string(t.size() > 1 ? ", " : "") + x.name + ":" + js(i18n::tr(x.key));
    t += "}";
    std::string page = kPage;
    replace(page, "__LANG__", i18n::code());
    replace(page, "__T__", t);
    return page;
}

} // namespace config_web
