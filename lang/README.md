# Translations of the interface

The game picker and the Tab settings are written in English; each `<code>.po` file here translates them into one
language (gettext's format, editable with [Poedit](https://poedit.net/) or any text editor). The loader reads the
folder `lang` next to `linuxloader.exe`.

- **Choosing the language**: Tab > Picker > Language, or `LANGUAGE` in the `[Launcher]` section of `linuxloader.ini`:
  `auto` (the system's language, the default), `en`, or the name of a file here without `.po` (`fr`). A name with a
  country (`pt_BR`) falls back on its language's file (`pt.po`) when it has none of its own.
- **A new language**: copy `linuxloader.pot` to `<code>.po` (`de.po`, `pt_BR.po`) and translate it. The header's
  `X-Language-Name` gives the name the Language list shows (`X-Language-Name: Deutsch`); without it the list shows the
  code. A text left empty, or marked fuzzy, stays in English.
- **After a change of the interface's texts**: `scripts/i18n/update-translations.sh` extracts them again into
  `linuxloader.pot` and merges it into each `.po`, where the new texts wait for a translation. It needs GNU gettext's
  `xgettext` and `msgmerge` (Git for Windows, MSYS2 and Linux distributions have them).

The font is Windows' Segoe UI: Latin, Greek and Cyrillic letters show, Chinese, Japanese or Korean ones would need
another font.
