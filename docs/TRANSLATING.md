# Translating Régie5

Régie5 ships in English, French, Spanish (`es`) and German (`de`). Each language
is one file in [`src/core/lang/`](../src/core/lang/), so adding one (Italian,
Portuguese…) does not touch the rest of the code. This guide uses Italian (`it`)
as the example.

**The Spanish and German translations were made with the help of AI.** If one of
them is your language, corrections are very welcome: a pull request on
`src/core/lang/es.cpp` or `de.cpp`, or an issue quoting the text and what it
should say. Words that are too long for a button or a card are worth reporting
too.

## 1. Copy the English file

```bash
cp src/core/lang/en.cpp src/core/lang/it.cpp
```

At the bottom of `it.cpp`, rename the language and give its rules:

```cpp
extern const Language kItalian = {
    "it", "Italiano", Plural::One, ',',
    {"dom", "lun", "mar", "mer", "gio", "ven", "sab"},                       // days, Sunday first
    {"gen", "feb", "mar", "apr", "mag", "giu", "lug", "ago", "set", "ott", "nov", "dic"},
    {"B", "KB", "MB", "GB", "TB", "PB"},                                     // sizes, ×1000 each
    "g",                                                                     // days in a duration: "2 g 4 h"
    "{wd} {d} {mon}", "{d} {mon} {y}", "{d1} – {d2} {mon2}", "{d1} {mon1} – {d2} {mon2}",
    kTexts, (int)(sizeof kTexts / sizeof kTexts[0]),
};
```

| Field | What it is |
|---|---|
| code | The ISO 639-1 code, as `--lang` takes it (`de`, `es`, `pt`…). A regional variant may use `pt-BR`; the console's `pt_BR` then picks it before `pt`. |
| name | The language's name in itself. |
| plural | `Plural::One` when only 1 takes the singular (English, German, Spanish), `Plural::ZeroAndOne` when 0 does too (French). |
| decimal | The decimal mark: `'.'` or `','`. |
| days, months | Short names, as they fit on a TV card. |
| size units | Bytes, kilobytes… up to petabytes (French writes `o`, `Ko`, `Mo`…). |
| day unit | The letter for days in a time left ("2 d 4 h"). |
| dates | Patterns with `{wd}` (day name), `{d}` (day of the month), `{mon}` (month name), `{y}` (year). In order: a day ("Thu, Oct 15"), a full date ("Oct 15, 2026"), a week within one month ("Oct 12 – 18", with `{d1}` `{d2}` `{mon1}` `{mon2}`), a week across two months ("Sep 28 – Oct 4"). |

## 2. Translate the texts

Every line of `kTexts` is `{Key, "text"}`; translate the text, keep the key.

- **Keep every `%d`, `%s`, `%lld`… in the same order.** The app fills them in
  that order (a count, a title, a size…). The tests check this for every text.
- Keep the TV in mind: texts are read from 3 m away, on cards and buttons of a
  fixed size. Prefer short words; check the screens (below).
- `Phone…` keys are the page a phone opens to fill in the configuration
  (Settings › From a phone).
- `WnItem1` to `WnItem5` are the "What's new" screen of the current version
  (Settings › What's new): short lines, one new thing each. A key whose text is
  `-` shows no line. A line that names Jellyfin is left out for Emby users.
- A key you leave out shows in English: a partial translation works, and the
  tests print how far it is.
- To see the English and your language side by side:

  ```bash
  scripts/list-strings.py it > build/strings-it.md
  ```

## 3. Add it to the list

In [`src/core/lang/languages.cpp`](../src/core/lang/languages.cpp), declare it and
add it to the list (English stays first):

```cpp
extern const Language kEnglish, kFrench, kSpanish, kGerman, kItalian;
...
        &kFrench,
        &kGerman,
        &kItalian,
```

The build picks up every file of `src/core/lang/` by itself.

## 4. Try it

On Linux, without any server:

```bash
scripts/run-linux.sh --demo --lang it
```

```bash
cmake --build build/linux && ctest --test-dir build/linux --output-on-failure
```

```bash
LANGS=it scripts/capture-states.sh
```

The last one captures every screen in every state (loading, empty, errors…)
into `build/states/`, so long texts and line breaks can be checked at a glance.

On the console, Régie5 follows the system language (Settings › Language on the
PS5); there is nothing else to set.

## Limits

- The typeface, [Inter](https://github.com/rsms/inter), covers the Latin, Greek
  and Cyrillic scripts. Japanese, Chinese, Korean, Arabic, Hebrew or Thai would
  need another font first: please open an issue before translating.
- Two plural forms only (singular and plural). A language with more forms
  (Polish, Russian, Czech…) can still be added with these two; say so in the
  pull request and the rule can be extended.
- Write **Jellyfin** and **Jelly5** where the English text has them: with an Emby
  server, Régie5 shows them as *Emby* and *Emby5* by itself (`i18n::set_emby`). Keep
  the sentence right for both names (no article or agreement that only fits one).
- Seerr's titles and overviews come in the language Régie5 asks for (its code);
  Sonarr, Radarr and Jellyfin give theirs in the language set on the server.

## Sending it

Open a pull request with your `src/core/lang/<code>.cpp` and the line in
`languages.cpp`. Screenshots of a few screens (from `capture-states.sh`) help the
review.
