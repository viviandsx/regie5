/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The languages of the interface. A new one: its file in this folder (a copy
 * of en.cpp, translated), and its line below (docs/TRANSLATING.md).
 */
#include "core/lang/language.h"

namespace i18n {

extern const Language kEnglish, kFrench;

const Language *const *all_languages(int *n)
{
    static const Language *const kAll[] = {
        &kEnglish,   /* first: the fallback */
        &kFrench,
    };
    *n = (int)(sizeof kAll / sizeof kAll[0]);
    return kAll;
}

} // namespace i18n
