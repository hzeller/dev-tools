#!/usr/bin/env bash

if [ $# -ne 1 ] ; then
  echo "Usage: $0 <clang-tidy-out>"
  echo "Remove all headers clang-tidy found to not be used directly"
  exit 1
fi

CLANG_TIDY_OUT=$1
awk '/included header.*is not used directly.*misc-include-cleaner/ {print $1 " " $5}' "$CLANG_TIDY_OUT" \
  | awk -F: '{gsub(/ /, "", $4); printf("sed -i %c/#include.*[<\"/]%s[>\"]/d%c %s\n", 39, $4, 39, $1);}'
