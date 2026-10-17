/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/i18n.h"

#include "core/lang/language.h"

#include <cctype>
#include <map>
#include <memory>
#include <string>

namespace i18n {

namespace {

const Language *s_lang = nullptr;
const char *s_texts[Count] = {};   /* the current language's, English where it has none */
const char *s_plain[Count] = {};   /* the same, before the media server's name is put in */
config::MediaServer s_media = config::MediaServer::Jellyfin;

/* A text with another media server's names in place of Jellyfin's, made once
 * and kept for the whole run: a pointer handed out by tr() never goes stale. */
const char *renamed(const char *text, config::MediaServer m)
{
    static std::map<std::pair<const char *, int>, std::unique_ptr<std::string>> made;
    const auto key = std::make_pair(text, (int)m);
    auto f = made.find(key);
    if (f != made.end())
        return f->second->c_str();
    std::string s = text;
    const bool plex = m == config::MediaServer::Plex;
    for (const auto &[from, to] : {std::pair<std::string, std::string>{"Jellyfin", plex ? "Plex" : "Emby"},
                                   {"Jelly5", plex ? "Plex" : "Emby5"}})
        for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size()))
            s.replace(at, from.size(), to);
    return made.emplace(key, std::make_unique<std::string>(std::move(s))).first->second->c_str();
}

void fill()
{
    for (int k = 0; k < Count; k++)
        s_texts[k] = s_media == config::MediaServer::Jellyfin ? s_plain[k] : renamed(s_plain[k], s_media);
}

std::string lower(const std::string &s)
{
    std::string out;
    for (char c : s)
        out += c == '_' ? '-' : (char)std::tolower((unsigned char)c);
    return out.substr(0, out.find('.'));   /* "fr_FR.UTF-8" -> "fr-fr" */
}

const Language *match(const std::string &code)
{
    int n = 0;
    const Language *const *all = all_languages(&n);
    const std::string c = lower(code);
    for (int i = 0; i < n; i++)
        if (c == lower(all[i]->code))
            return all[i];
    for (int i = 0; i < n; i++)
        if (c.size() >= 2 && c.compare(0, 2, lower(all[i]->code), 0, 2) == 0)
            return all[i];
    return nullptr;
}

} // namespace

const Language &find(const std::string &code)
{
    const Language *l = match(code);
    int n = 0;
    return l ? *l : *all_languages(&n)[0];
}

bool known(const std::string &code) { return match(code) != nullptr; }

void set_lang(const Language &l)
{
    int n = 0;
    const Language &en = *all_languages(&n)[0];
    for (int k = 0; k < Count; k++)
        s_plain[k] = "";
    for (const Language *src : {&en, &l})
        for (int i = 0; i < src->count; i++)
            if (src->texts[i].key >= 0 && src->texts[i].key < Count && src->texts[i].text)
                s_plain[src->texts[i].key] = src->texts[i].text;
    s_lang = &l;
    fill();
}

const char *media_server() { return config::media_name(s_media); }
config::MediaServer media() { return s_media; }
bool emby() { return s_media == config::MediaServer::Emby; }
bool plex() { return s_media == config::MediaServer::Plex; }

void set_media(config::MediaServer m)
{
    language();
    s_media = m;
    fill();
}

const Language &language()
{
    if (!s_lang)
        set_lang(find("en"));
    return *s_lang;
}

const char *code() { return language().code; }

bool singular(long long n) { return language().plural == Plural::ZeroAndOne ? n <= 1 && n >= 0 : n == 1; }

const char *tr(S key)
{
    language();
    if (key < 0 || key >= Count)
        return "";
    return s_texts[key];
}

const char *plain(S key)
{
    language();
    if (key < 0 || key >= Count)
        return "";
    return s_plain[key];
}

} // namespace i18n
