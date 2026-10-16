#!/bin/sh
# ----------------------------------------------------------------------------
#   Install (or remove) the v32c++ documents in one language
#
#   usage: scripts/install_docs.sh <lang> <destination directory>
#          scripts/install_docs.sh --remove <destination directory>
#
#   <lang> is the DEST_LANG of the Makefile:
#     all  every document in every language, as they are (the default)
#     en   the English documents only
#     es   the Spanish documents (README.es.md, docs/*.es.md) under their
#     fr   plain names (README.md, ...), the French likewise; documents that
#          exist only in English (docs/DESIGN_NOTES.md, ...) are installed
#          alongside
#
#   For a single language the "language links" line of each document (the one
#   ending in <!-- langlinks -->) is dropped, since the other versions are not
#   installed, and a link to X.es.md / X.fr.md is rewritten to X.md.
#
#   --remove deletes every file this script can install (all languages) from
#   the directory, then the directory if that leaves it empty.
#
#   Like the installed layout of the CMake build, the documents go flat into
#   the destination directory. Plain POSIX sh, run from the top of the source
#   tree.
# ----------------------------------------------------------------------------
set -eu

if [ $# -ne 2 ]; then
    echo "usage: $0 <all|en|es|fr|--remove> <destination directory>" >&2
    exit 2
fi
lang=$1
dest=$2

[ -f README.md ] && [ -d docs ] || { echo "install_docs: run this from the top of the v32c++ source tree" >&2; exit 1; }

if [ "$lang" = "--remove" ]; then
    for f in README README.es README.fr; do rm -f "$dest/$f.md"; done
    rm -f "$dest/LICENSE"
    for f in docs/*.md; do rm -f "$dest/${f##*/}"; done
    rmdir "$dest" 2>/dev/null && echo "removed empty $dest"
    exit 0
fi

case "$lang" in
    all|en|es|fr) ;;
    *) echo "install_docs: DEST_LANG must be all, en, es or fr (not '$lang')" >&2; exit 2 ;;
esac

umask 022
install -d "$dest"

# put <source> <installed name>: copy, with the single-language rewrites
put() {
    src=$1
    name=$2
    case "$lang" in
        all) install -m 644 "$src" "$dest/$name" ;;
        *)   sed -e '/<!-- langlinks -->/d' \
                 -e 's/\.es\.md/.md/g' -e 's/\.fr\.md/.md/g' "$src" > "$dest/$name.tmp" || exit 1
             chmod 644 "$dest/$name.tmp"
             mv "$dest/$name.tmp" "$dest/$name" ;;
    esac
}

[ -f LICENSE ] && install -m 644 LICENSE "$dest/LICENSE"

for src in README.md docs/*.md; do
    base=${src##*/}
    case "$base" in
        *.es.md|*.fr.md) continue ;;   # handled with its English original below
    esac
    stem=${base%.md}
    dir=${src%"$base"}
    case "$lang" in
        all)
            put "$src" "$base"
            for l in es fr; do
                [ -f "$dir$stem.$l.md" ] && put "$dir$stem.$l.md" "$stem.$l.md"
            done ;;
        en)
            put "$src" "$base" ;;
        *)
            if [ -f "$dir$stem.$lang.md" ]; then
                put "$dir$stem.$lang.md" "$base"
            else
                put "$src" "$base"
            fi ;;
    esac
done
