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

// Location: https://github.com/hzeller/dev-tools (2024-09-25)

// Script that moves a particular include as the first header in a file.

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

static int usage(const char *progname) {
  fprintf(stderr, "Usage: %s <header> <file>\n", progname);
  fprintf(stderr, "Example\n\t%s foo/bar.h src/foo/bar.cc\n\n", progname);
  fprintf(stderr, "If an include of #include \"foo/bar.h\" is found in "
          "src/foo/bar.cc, then it is moved before the first include.\n");
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
  const std::string expected_include =
    std::string("#include \"") + argv[1] + "\"";
  const std::string file_to_modify = argv[2];

  auto content_or = GetContent(file_to_modify);
  if (!content_or.has_value()) {
    return EXIT_FAILURE;
  }
  const std::string &content = *content_or;
  const size_t first_header = content.find("#include ");
  if (first_header == std::string::npos) {
    std::cerr << file_to_modify << ": No headers found.\n";
    return EXIT_SUCCESS;
  }

  const size_t our_header = content.find(expected_include);
  if (our_header == std::string::npos) {
    std::cerr << file_to_modify << ": Not having " << expected_include << "\n";
    return EXIT_SUCCESS;
  }

  if (our_header == first_header) {
    //std::cerr << file_to_modify << ": Header already in right place.\n";
    return EXIT_SUCCESS;
  }

  // Find the end of the line for our header (there might be //-comments)
  size_t our_header_end = our_header + expected_include.size();
  while (our_header_end < content.size() && content[our_header_end] != '\n') {
    ++our_header_end;
  }
  const std::string_view header_to_move(content.data() + our_header,
                                        our_header_end - our_header + 1);

  // Re-assemble file in the new order.
  const std::string tmp_file_name = file_to_modify + ".tmp";
  FILE *const tmp_out = fopen(tmp_file_name.c_str(), "wb");

  size_t written = 0;
  // Up to first header.
  written += fwrite(content.data(), 1, first_header, tmp_out);

  // Add our header, including whatever comments were following it
  written += fwrite(header_to_move.data(), 1, header_to_move.size(), tmp_out);

  // If there was not already a double-newline after that header, add it.
  if (header_to_move[header_to_move.size() - 2] != '\n') {
    fwrite("\n", 1, 1, tmp_out);  // Don't add to written as it changes size.
  }

  // Continue the snippet from formerly first header to our header
  written += fwrite(content.data() + first_header, 1,
                    our_header - first_header, tmp_out);

  // Skip the old position of our header and write rest of file.
  const char *rest_of_file = header_to_move.data() + header_to_move.size();
  const char *end_of_file = content.data() + content.size();
  written += fwrite(rest_of_file, 1, end_of_file - rest_of_file, tmp_out);

  if (written != content.size()) {
    std::cerr << file_to_modify << ": Size we could write is not size of "
      "original file (" << written << " vs. " << content.size() << ")\n";
    return EXIT_FAILURE;
  }
  if (fclose(tmp_out) != 0) return EXIT_FAILURE;

  return (rename(tmp_file_name.c_str(), file_to_modify.c_str()) == 0)
    ? EXIT_SUCCESS
    : EXIT_FAILURE;
}
