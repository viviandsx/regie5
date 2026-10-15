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
const SERVICES = [["sonarr","Sonarr",true],["radarr","Radarr",true],["seerr","Seerr",false],["jellyfin","Jellyfin",false],["emby","Emby",false],["glances","Glances",false]];
const PORTS = {sonarr: 8989, radarr: 7878, seerr: 5055, jellyfin: 8096, emby: 8096, glances: 61208};
const query = new URLSearchParams(location.search);
const code = query.get("c") || (prompt(T.askCode) || "").replace(/\D/g, "");
/* m: the media server chosen in the welcome screens (jellyfin, emby, none):
 * the other one's section is not shown, and so not sent (left as it is). */
const media = query.get("m");
const shown = key => !media || (key !== "jellyfin" && key !== "emby") || key === media;
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
    (list.length ? list : [{enabled: key !== "glances" && (key !== "emby" || media === "emby")}]).forEach((d, i) => holder.appendChild(instance(key, multi, d, list.length ? i : -1)));
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
