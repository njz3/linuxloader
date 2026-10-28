#!/usr/bin/env bash
# Updates the interface's translations after its texts changed: extracts the texts tr() translates (tr("...") and
# N_("...")) into lang/linuxloader.pot, then merges it into each lang/*.po, where the new and changed texts wait for a
# translation (Poedit, or any .po editor). A new language: copy lang/linuxloader.pot to lang/<code>.po (de.po) and
# translate it, its header's X-Language-Name giving the name the Language list shows.
# Needs GNU gettext's xgettext and msgmerge (Git for Windows, MSYS2 and Linux distributions have them).
set -e
cd "$(dirname "$0")/../.."

xgettext --from-code=UTF-8 --language=C++ --keyword=tr --keyword=N_ --add-location=file \
    --package-name="Lindbergh Loader" --msgid-bugs-address="https://github.com/njz3/linuxloader/issues" \
    -o lang/linuxloader.pot src/loader/gui/*.cpp src/loader/config/gameOptions.c

for po in lang/*.po; do
    msgmerge --quiet --update --backup=none "$po" lang/linuxloader.pot
    msgattrib --no-obsolete -o "$po" "$po"
    echo "$po: $(msgfmt --check --statistics -o /dev/null "$po" 2>&1)"
done
