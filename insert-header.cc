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

// Location: https://github.com/hzeller/dev-tools (2024-10-18)

// Script that moves a particular include as the first header in a file.

#include <unistd.h>

#include <cctype>
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
  fprintf(stderr, "Usage: %s <header> [-q] [-e<explanation>] <file>...\n",
          progname);
  fprintf(stderr,
          "\nSimple way to insert a header into c/c++ file(s) if not there "
          "already.\nHeader can be simple string (in which case it is "
          "included with \"...\") or bracketed with '<...>'.\n"
          "If header starts with '<', it is attempted to be inserted near "
          "an angle-bracket header.\n\n"
          "Example:\n"
          "\t  %s '<vector>' foo.cc bar.cc\n"
          "\twill insert `#include <vector>` before the first angle include\n"
          "\tinto files foo.cc and bar.cc;\n\n\tSimilarly,\n"
          "\t  %s 'hello/world.h' foo.cc bar.cc\n"
          "\twill insert `#include \"hello/world.h\"` before the second quote "
          "include.\n\n"
          "Options:\n"
          "\t-q              : quiet. Less verbose about info messages.\n"
          "\t-e<explanation> : Print explanation-text on successful header-add\n"
          "\t-a<width>       : When printing message: align filename width\n",
          progname, progname);
  return EXIT_FAILURE;
}

static std::optional<std::string> GetContent(FILE *f) {
  if (!f) {
    return std::nullopt;
  }
  std::string result;
  char buf[4096];
  while (const size_t r = fread(buf, 1, sizeof(buf), f)) {
    result.append(buf, r);
  }
  fclose(f);
  return result;
}

static std::optional<std::string> GetContent(const std::string &path) {
  FILE *const file_to_read = fopen(path.c_str(), "rb");
  if (!file_to_read) {
    fprintf(stderr, "%s: can't open: %s\n", path.c_str(), strerror(errno));
    return std::nullopt;
  }
  return GetContent(file_to_read);
}

static size_t FindBestInsertPos(std::string_view content, bool is_angle_inc) {
  // Depending on what header type to insert, let's put it in the right group.

  // Angle header: just before the first angle header we find.
  const size_t angle_header_inspos = content.find("\n#include <");

  // Quote header: prefer inserting before the second one, as the first one
  // might be the implementation header.
  const size_t first_quote_header_inspos = content.find("\n#include \"");
  size_t quote_header_inspos = first_quote_header_inspos;
  if (quote_header_inspos != std::string::npos) {
    const size_t second_quote_header =
        content.find("\n#include \"", quote_header_inspos + 1);
    if (second_quote_header != std::string::npos) {
      quote_header_inspos = second_quote_header;
    }
  }

  size_t insert_pos = is_angle_inc ? angle_header_inspos : quote_header_inspos;
  if (insert_pos == std::string::npos) {
    insert_pos = is_angle_inc ? first_quote_header_inspos : angle_header_inspos;
  }
  if (insert_pos == std::string::npos) {
    return 0;
  }

  // All our searches start with the preceeding newline; insert where '#' is.
  return insert_pos + 1;
}

static bool ModifyFile(const std::string &file_to_modify, bool is_angle_inc,
                       const std::string &insert_header,
                       bool quiet,
                       int align_len,
                       const std::string &explanation) {
  auto content_or = GetContent(file_to_modify);
  if (!content_or.has_value()) {
    return false;
  }
  const std::string &content = *content_or;

  const std::string report_filename = file_to_modify + ":";
  if (content.find(insert_header) != std::string::npos) {
    if (!quiet) {
      fprintf(stderr, "%*s %s already there\n", -align_len,
              report_filename.c_str(), insert_header.c_str());
    }
    return true;
  }

  if (!explanation.empty()) {
    fprintf(stdout, "%*s %s %s\n", -align_len, report_filename.c_str(),
            insert_header.c_str(), explanation.c_str());
  }

  const size_t insert_pos = FindBestInsertPos(content, is_angle_inc);

  // Re-assemble file.
  const std::string tmp_file_name = file_to_modify + ".tmp";
  FILE *const tmp_out = fopen(tmp_file_name.c_str(), "wb");

  size_t written = 0;
  // Up to first header.
  written += fwrite(content.data(), 1, insert_pos, tmp_out);

  written += fwrite(insert_header.data(), 1, insert_header.size(), tmp_out);
  written += fwrite("\n", 1, 1, tmp_out);
  written += fwrite(content.data() + insert_pos, 1, content.size() - insert_pos,
                    tmp_out);

  const size_t expected_size = content.size() + insert_header.size() + 1;
  if (written != expected_size) {
    std::cerr << file_to_modify
              << ": Unexpected final size "
                 "original file ("
              << written << " vs. " << expected_size << ")\n";
    return false;
  }
  if (fclose(tmp_out) != 0) {
    return false;
  }

  return rename(tmp_file_name.c_str(), file_to_modify.c_str()) == 0;
}

int main(int argc, char *argv[]) {
  bool quiet = false;
  std::string explanation;
  int print_alignment = 40;
  int opt;
  while ((opt = getopt(argc, argv, "qe:")) != -1) {
    switch (opt) {
      case 'q':
        quiet = true;
        break;
      case 'e':
        explanation = optarg;
        break;
      case 'a':
        print_alignment = atoi(optarg);
        break;
      default:
        return usage(argv[0]);
    }
  }

  const int header_arg = optind;
  const int start_files = optind + 1;

  if (header_arg >= argc) {
    std::cerr << "Expected header to include\n";
    return usage(argv[0]);
  }

  std::string_view header(argv[header_arg]);
  while (!header.empty() && isspace(header.front())) {
    header.remove_prefix(1);
  }
  while (!header.empty() && isspace(header.back())) {
    header.remove_suffix(1);
  }
  if (header.length() < 2) {
    return usage(argv[0]);
  }

  const bool is_angle_inc = (header[0] == '<');
  const bool has_quote_prefix_already = (header[0] == '"');
  const std::string hash_include("#include ");
  const std::string inc_header(header);
  const std::string insert_header =
      is_angle_inc || has_quote_prefix_already
          ? hash_include + inc_header
          : hash_include + "\"" + inc_header + "\"";
  if (is_angle_inc && insert_header.find_first_of('>') == std::string::npos) {
    std::cerr << "Missing '>' at include\n";
    return EXIT_FAILURE;
  }

  if (start_files >= argc && !quiet) {
    std::cerr << "No files provided\n";
  }

  bool success = true;
  for (int i = start_files; i < argc; ++i) {
    success &= ModifyFile(argv[i], is_angle_inc, insert_header,
                          quiet, print_alignment, explanation);
  }
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
