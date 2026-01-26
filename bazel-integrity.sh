#!/bin/sh


if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <file> [<file>...]"
    exit 1
fi

while [ $# -ne 0 ] ; do
    if [ $# -eq 1 ] ; then
	WITH_COMMA=""
    else
	WITH_COMMA=","
    fi
    HASH=$(nix-shell -p openssl --run "openssl dgst -sha256 -binary \"$1\" | openssl base64 -A | sed 's/^/sha256-/'")
    echo "        \"$(basename $1)\": \"$HASH\"$WITH_COMMA"
    shift 1
done
