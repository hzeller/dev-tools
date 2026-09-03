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
INSERT_HEADER="${MY_DIR}/insert-header"

if [ ! -x "${INSERT_HEADER}" ]; then
  c++ -std=c++20 -o "${INSERT_HEADER}" "${MY_DIR}/insert-header.cc"
fi

TMPDIR=$(mktemp -d /tmp/insert_header_test.XXXXXX)
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

echo "Running insert-header tests..."

echo "Test: Insert C++ header into C++ block and sort that block only"
cat <<'EOF' > "${TMPDIR}/t1.cc"
#include <unistd.h>

#include <string>

#include "z_header.h"
#include "a_header.h"
EOF

"${INSERT_HEADER}" '<vector>' "${TMPDIR}/t1.cc"

EXPECTED_TEST1=$(cat <<'EOF'
#include <unistd.h>

#include <string>
#include <vector>

#include "z_header.h"
#include "a_header.h"
EOF
)
assert_file_content "${TMPDIR}/t1.cc" "${EXPECTED_TEST1}"

echo "Test: Insert C header into C block and sort that block only"
cat <<'EOF' > "${TMPDIR}/t2.cc"
#include <unistd.h>

#include <vector>
EOF

"${INSERT_HEADER}" '<stdio.h>' "${TMPDIR}/t2.cc"

EXPECTED_TEST2=$(cat <<'EOF'
#include <stdio.h>
#include <unistd.h>

#include <vector>
EOF
)
assert_file_content "${TMPDIR}/t2.cc" "${EXPECTED_TEST2}"

echo "Test: Insert quote header into quote block and sort that block only"
cat <<'EOF' > "${TMPDIR}/t3.cc"
#include <vector>

#include "z_header.h"
#include "a_header.h"
EOF

"${INSERT_HEADER}" 'm_header.h' "${TMPDIR}/t3.cc"

EXPECTED_TEST3=$(cat <<'EOF'
#include <vector>

#include "a_header.h"
#include "m_header.h"
#include "z_header.h"
EOF
)
assert_file_content "${TMPDIR}/t3.cc" "${EXPECTED_TEST3}"

echo "Test: Insert own header at top of file when no own header exists"
cat <<'EOF' > "${TMPDIR}/my_tool.cc"
#include <vector>

#include "a_header.h"
EOF

"${INSERT_HEADER}" 'my_tool.h' "${TMPDIR}/my_tool.cc"

EXPECTED_TEST4=$(cat <<'EOF'
#include "my_tool.h"

#include <vector>

#include "a_header.h"
EOF
)
assert_file_content "${TMPDIR}/my_tool.cc" "${EXPECTED_TEST4}"

echo "Test: Preserve own header at top when inserting a new quote header"
cat <<'EOF' > "${TMPDIR}/my_service.cc"
#include "my_service.h"

#include <vector>

#include "z_header.h"
EOF

"${INSERT_HEADER}" 'a_header.h' "${TMPDIR}/my_service.cc"

EXPECTED_TEST5=$(cat <<'EOF'
#include "my_service.h"

#include <vector>

#include "a_header.h"
#include "z_header.h"
EOF
)
assert_file_content "${TMPDIR}/my_service.cc" "${EXPECTED_TEST5}"

echo "Test: Header already present (file remains untouched)"
cat <<'EOF' > "${TMPDIR}/t6.cc"
#include <vector>
EOF

"${INSERT_HEADER}" -q '<vector>' "${TMPDIR}/t6.cc"

EXPECTED_TEST6=$(cat <<'EOF'
#include <vector>
EOF
)
assert_file_content "${TMPDIR}/t6.cc" "${EXPECTED_TEST6}"

echo "Test: Insert own header foo.h at top of test file foo_test.cc"
cat <<'EOF' > "${TMPDIR}/foo_test.cc"
#include <vector>

#include "a_header.h"
EOF

"${INSERT_HEADER}" 'foo.h' "${TMPDIR}/foo_test.cc"

EXPECTED_TEST7=$(cat <<'EOF'
#include "foo.h"

#include <vector>

#include "a_header.h"
EOF
)
assert_file_content "${TMPDIR}/foo_test.cc" "${EXPECTED_TEST7}"

echo "Test: Preserve own header foo.h at top of foo_test.cc when inserting a quote header"
cat <<'EOF' > "${TMPDIR}/bar_test.cc"
#include "bar.h"

#include <vector>

#include "z_header.h"
EOF

"${INSERT_HEADER}" 'a_header.h' "${TMPDIR}/bar_test.cc"

EXPECTED_TEST8=$(cat <<'EOF'
#include "bar.h"

#include <vector>

#include "a_header.h"
#include "z_header.h"
EOF
)
assert_file_content "${TMPDIR}/bar_test.cc" "${EXPECTED_TEST8}"

echo "Test: Create new block for missing cpp category between existing blocks with newline separation"
cat <<'EOF' > "${TMPDIR}/new_block_cpp.cc"
#include <stdio.h>

#include "a_header.h"
EOF

"${INSERT_HEADER}" '<vector>' "${TMPDIR}/new_block_cpp.cc"

EXPECTED_TEST9=$(cat <<'EOF'
#include <stdio.h>

#include <vector>

#include "a_header.h"
EOF
)
assert_file_content "${TMPDIR}/new_block_cpp.cc" "${EXPECTED_TEST9}"

echo "Test: Create new block for missing c category before existing blocks with newline separation"
cat <<'EOF' > "${TMPDIR}/new_block_c.cc"
#include <vector>

#include "a_header.h"

int here_is_some_code();
EOF

"${INSERT_HEADER}" '<stdio.h>' "${TMPDIR}/new_block_c.cc"

EXPECTED_TEST10=$(cat <<'EOF'
#include <stdio.h>

#include <vector>

#include "a_header.h"

int here_is_some_code();
EOF
)
assert_file_content "${TMPDIR}/new_block_c.cc" "${EXPECTED_TEST10}"

echo "Test: Create new block for missing quote category after existing blocks with newline separation"
cat <<'EOF' > "${TMPDIR}/new_block_quote.cc"
#include <stdio.h>

#include <vector>

int here_is_some_code();
EOF

"${INSERT_HEADER}" 'b_header.h' "${TMPDIR}/new_block_quote.cc"

EXPECTED_TEST11=$(cat <<'EOF'
#include <stdio.h>

#include <vector>

#include "b_header.h"

int here_is_some_code();
EOF
)
assert_file_content "${TMPDIR}/new_block_quote.cc" "${EXPECTED_TEST11}"

echo "Test: Create new quote block when only an own header exists"
cat <<'EOF' > "${TMPDIR}/own_only.cc"
#include "own_only.h"

int here_is_some_code();
EOF

"${INSERT_HEADER}" 'a_header.h' "${TMPDIR}/own_only.cc"

EXPECTED_TEST12=$(cat <<'EOF'
#include "own_only.h"

#include "a_header.h"

int here_is_some_code();
EOF
)
assert_file_content "${TMPDIR}/own_only.cc" "${EXPECTED_TEST12}"

echo "Test: Insert header at the beginning of a file"
cat <<'EOF' > "${TMPDIR}/bar.cc"

int here_is_some_code();
EOF

"${INSERT_HEADER}" 'a_header.h' "${TMPDIR}/bar.cc"

EXPECTED_TEST13=$(cat <<'EOF'
#include "a_header.h"

int here_is_some_code();
EOF
)
assert_file_content "${TMPDIR}/bar.cc" "${EXPECTED_TEST13}"

echo "Test: Insert header at the beginning of a file, no pre-existing empty line"
cat <<'EOF' > "${TMPDIR}/bar.cc"
int here_is_some_code();
EOF

"${INSERT_HEADER}" 'a_header.h' "${TMPDIR}/bar.cc"

EXPECTED_TEST13=$(cat <<'EOF'
#include "a_header.h"

int here_is_some_code();
EOF
)
assert_file_content "${TMPDIR}/bar.cc" "${EXPECTED_TEST13}"

echo "ALL insert-header tests PASSED!"
