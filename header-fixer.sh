#!/usr/bin/env bash

if [ $# -ne 2 ]; then
    cat <<EOF
Usage: $0 <clang-tidy-out> <replacement-list-file>

The clang-tidy-out file is a clang-tidy.out file as generated bu
ryn-clang-tidy-cached.cc

The replacment list file has two columns, that are whitespace delimited.

The first column contains a regular expression for a symbol that is missing,
the second column says corresponding header to include. If there are multiple
rows that include the same header, the header only has to be mentioned the
first time in that column.

Each line will be translated to a one-liner script to insert the
corresponding header.

Tip:
To get an overview list of symbols that are used without their header, run

awk '/no header providing.*misc-include-cleaner/ {print \$6}' my_clang-tidy.out | sort | uniq -c | sort -n
EOF
    exit 0
fi

BASEDIR=$(dirname $0)
TIDY_OUT=$1
REPLACEMENT_FILE=$2

awk -vBASEDIR="$BASEDIR" -vTIDY_OUT="$TIDY_OUT" -f- "$REPLACEMENT_FILE" <<'EOF'
{
  if ($1 == "") {
    next;
  }
  if ($2 != "") {
    CURRENT_HEADER=$2;
  }
  printf("%s/insert-header.cc '%s' $(awk -F: '/no header providing \"%s\".*misc-include-cleaner/ {print $1}' %s | sort | uniq)\n",
  	       BASEDIR, CURRENT_HEADER, $1, TIDY_OUT);
}
EOF
