#!/bin/sh
# Regenerates src/stdstring.c from lib/string: the text of v32c++'s
# built-in <string> header, embedded in the transpiler so that
# `#include <string>` needs no file on disk.
#
# Run from the project root after editing lib/string:
#     sh tools/embed-header.sh
set -e
out=src/stdstring.c
{
    echo '/* GENERATED from lib/string by tools/embed-header.sh -- do not edit.'
    echo ' * Edit lib/string and run the script again. */'
    echo '#include <stddef.h>'
    echo '#include "stdstring.h"'
    echo
    echo 'const char *const g_stdstring_lines[] = {'
    sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/    "/' -e 's/$/",/' lib/string
    echo '    NULL'
    echo '};'
} > "$out"
echo "wrote $out"
