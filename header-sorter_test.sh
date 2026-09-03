#!/usr/bin/env bash
# Copyright 2026 Henner Zeller <h.zeller@acm.org>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euo pipefail

MY_DIR=$(cd "$(dirname "$0")" && pwd)
HEADER_SORTER="${MY_DIR}/header-sorter"

if [ ! -x "${HEADER_SORTER}" ]; then
  c++ -std=c++20 -o "${HEADER_SORTER}" "${MY_DIR}/header-sorter.cc"
fi

TMPDIR=$(mktemp -d /tmp/header_sorter_test.XXXXXX)
trap 'rm -rf "$TMPDIR"' EXIT

assert_file_content() {
  local file="$1"
  local expected="$2"
  local actual
  actual=$(cat "$file")
  if [ "$actual" != "$expected" ]; then
    echo "Test failed for $file:"
    echo "--- Expected ---"
    echo "$expected"
    echo "--- Actual ---"
    echo "$actual"
    echo "--- Diff ---"
    diff -u <(echo "$expected") <(echo "$actual") || true
    exit 1
  fi
}

echo "Running header-sorter tests..."

echo "Test: All blocks sorted by default"
cat <<'EOF' > "${TMPDIR}/baz.cc"
#include "foo/bar/baz.h"
#include <unistd.h>
#include <stdio.h>

#include <vector>
#include <algorithm>

#include "z_header.h"
#include "a_header.h"
EOF

"${HEADER_SORTER}" "${TMPDIR}/baz.cc"

EXPECTED_TEST1=$(cat <<'EOF'
#include "foo/bar/baz.h"
#include <stdio.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include "a_header.h"
#include "z_header.h"
EOF
)
assert_file_content "${TMPDIR}/baz.cc" "${EXPECTED_TEST1}"

echo "Test: Selective sorting with -s c -s c++ (leave quote headers unsorted)"
cat <<'EOF' > "${TMPDIR}/selective1.cc"
#include <unistd.h>
#include <stdio.h>

#include <vector>
#include <algorithm>

#include "z_header.h"
#include "a_header.h"
EOF

"${HEADER_SORTER}" -s c -s c++ "${TMPDIR}/selective1.cc"

EXPECTED_TEST2=$(cat <<'EOF'
#include <stdio.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include "z_header.h"
#include "a_header.h"
EOF
)
assert_file_content "${TMPDIR}/selective1.cc" "${EXPECTED_TEST2}"

echo "Test: Selective sorting with -s quote (only quote headers sorted)"
cat <<'EOF' > "${TMPDIR}/selective2.cc"
#include <unistd.h>
#include <stdio.h>

#include "z_header.h"
#include "a_header.h"
EOF

"${HEADER_SORTER}" -s quote "${TMPDIR}/selective2.cc"

EXPECTED_TEST3=$(cat <<'EOF'
#include <unistd.h>
#include <stdio.h>

#include "a_header.h"
#include "z_header.h"
EOF
)
assert_file_content "${TMPDIR}/selective2.cc" "${EXPECTED_TEST3}"

echo "Test: Preserving kOwnHeader at top of file"
cat <<'EOF' > "${TMPDIR}/my_tool.cc"
#include "my_tool.h"
#include "z_header.h"
#include "a_header.h"
EOF

"${HEADER_SORTER}" "${TMPDIR}/my_tool.cc"

EXPECTED_TEST4=$(cat <<'EOF'
#include "my_tool.h"
#include "a_header.h"
#include "z_header.h"
EOF
)
assert_file_content "${TMPDIR}/my_tool.cc" "${EXPECTED_TEST4}"

echo "ALL header-sorter tests PASSED!"
