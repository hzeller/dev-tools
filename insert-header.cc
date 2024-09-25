#include <cstdlib>
#if 0  // Invoke with /bin/sh or simply add executable bit on this file on Unix.
B=${0%%.cc}; [ "$B" -nt "$0" ] || c++ -std=c++20 -o"$B" "$0" && exec "$B" "$@";
#endif
// Copyright 2024 Henner Zeller <h.zeller@acm.org>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Location: https://github.com/hzeller/dev-tools (2024-09-24)

// Script that moves a particular include as the first header in a file.

#include <stdio.h>

#include <cstddef>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

static int usage(const char *progname) {
  fprintf(stderr, "Usage: %s <file> <header>\n", progname);
  fprintf(stderr, "Insert a header to file if not already.\n"
          "Header can be soem simple string or bracketed with '<...>'\n"
          "If header starts with '<', it is attempted to be inserted near\n"
          "an angle-bracket headder\n");
  return EXIT_FAILURE;
}

std::optional<std::string> GetContent(FILE *f) {
  if (!f) return std::nullopt;
  std::string result;
  char buf[4096];
  while (const size_t r = fread(buf, 1, sizeof(buf), f)) {
    result.append(buf, r);
  }
  fclose(f);
  return result;
}

std::optional<std::string> GetContent(const std::string &path) {
  FILE *const file_to_read = fopen(path.c_str(), "rb");
  if (!file_to_read) {
    fprintf(stderr, "%s: can't open: %s\n", path.c_str(),
            strerror(errno));
    return std::nullopt;
  }
  return GetContent(file_to_read);
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    return usage(argv[0]);
  }
  const std::string file_to_modify = argv[1];
  const bool is_angle_inc = argv[2][0] == '<';
  const std::string insert_include =
      is_angle_inc ? std::string("#include ") + argv[2]
                   : std::string("#include \"") + argv[2] + "\"";
  if (is_angle_inc && insert_include.back() != '>') {
    std::cerr << "Missing '>' at include\n";
    return EXIT_FAILURE;
  }

  auto content_or = GetContent(file_to_modify);
  if (!content_or.has_value()) {
    return EXIT_FAILURE;
  }
  const std::string &content = *content_or;

  if (content.find(insert_include) != std::string::npos) {
    std::cerr << file_to_modify << ": " << insert_include << " already there\n";
    return EXIT_SUCCESS;
  }

  // Depending on what header type to insert, let's put it in the right group.
  const size_t first_quote_header = content.find("#include \"");
  const size_t first_angle_header = content.find("#include <");
  size_t insert_pos = is_angle_inc ? first_angle_header : first_quote_header;
  if (insert_pos == std::string::npos) {
    insert_pos = is_angle_inc ? first_quote_header : first_angle_header;
  }
  if (insert_pos == std::string::npos) {
    insert_pos = 0;
  }
  // Re-assemble file.
  const std::string tmp_file_name = file_to_modify + ".tmp";
  FILE *const tmp_out = fopen(tmp_file_name.c_str(), "wb");

  size_t written = 0;
  // Up to first header.
  written += fwrite(content.data(), 1, insert_pos, tmp_out);

  written += fwrite(insert_include.data(), 1, insert_include.size(), tmp_out);
  written += fwrite("\n", 1, 1, tmp_out);
  written += fwrite(content.data() + insert_pos, 1,
                    content.size() - insert_pos, tmp_out);

  const size_t expected_size = content.size() + insert_include.size() + 1;
  if (written != expected_size) {
    std::cerr << file_to_modify << ": Unexpected final size "
      "original file (" << written << " vs. " << expected_size << ")\n";
    return EXIT_FAILURE;
  }
  if (fclose(tmp_out) != 0) return EXIT_FAILURE;

  return (rename(tmp_file_name.c_str(), file_to_modify.c_str()) == 0)
    ? EXIT_SUCCESS
    : EXIT_FAILURE;
}
