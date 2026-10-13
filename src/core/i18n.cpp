/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "core/i18n.h"

#include "core/lang/language.h"

#include <cctype>

namespace i18n {

namespace {

const Language *s_lang = nullptr;
const char *s_texts[Count] = {};   /* the current language's, English where it has none */

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
        s_texts[k] = "";
    for (const Language *src : {&en, &l})
        for (int i = 0; i < src->count; i++)
            if (src->texts[i].key >= 0 && src->texts[i].key < Count && src->texts[i].text)
                s_texts[src->texts[i].key] = src->texts[i].text;
    s_lang = &l;
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

} // namespace i18n
