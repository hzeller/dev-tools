#if 0  // Invoke with /bin/sh or simply add executable bit on this file on Unix.
B=${0%%.cc}; [ "$B" -nt "$0" ] || c++ -std=c++20 -o"$B" "$0" && exec "$B" "$@";
#endif
// Copyright 2026 Henner Zeller <h.zeller@acm.org>
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

// Location: https://github.com/hzeller/dev-tools (2026-09-01)

// Script that sorts blocks of headers

#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "header-utils.h"

static int usage(const char *progname) {
  fprintf(stderr, "Usage: %s [-s c|c++|quote] <file>...\n", progname);
  return EXIT_FAILURE;
}

struct Options {
  bool sort_c = false;
  bool sort_cpp = false;
  bool sort_quote = false;
};

static bool ShouldSort(IncludeType type, const Options &options) {
  switch (type) {
    case IncludeType::kAngleC:
      return options.sort_c;
    case IncludeType::kAngleCpp:
      return options.sort_cpp;
    case IncludeType::kQuote:
      return options.sort_quote;
    default:
      return false;
  }
}

static bool ModifyFile(const std::string &file_to_modify,
                       const Options &options) {
  auto content_or = GetContent(file_to_modify);
  if (!content_or.has_value()) {
    return false;
  }
  const std::string &content = *content_or;

  const std::string tmp_file_name = file_to_modify + ".tmp";
  FILE *const tmp_out = fopen(tmp_file_name.c_str(), "wb");
  if (!tmp_out) {
    fprintf(stderr, "%s: can't open tmp file %s: %s\n", file_to_modify.c_str(),
            tmp_file_name.c_str(), strerror(errno));
    return false;
  }

  size_t written = 0;

  // Find blocks of lines starting with #include. A block is defined as a
  // sequence of lines of the same include type (e.g. C angle bracket vs.
  // C++ angle bracket vs. double quote) without a newline or non-include line.

  // Emit file. Lines not starting with #include are emitted as-is.
  // Lines starting with #include are first remembered as a vector of
  // string_views representing the lines, then sorted (if enabled for the
  // block type) and emitted.
  std::vector<std::string_view> include_block;
  IncludeType current_block_type = IncludeType::kNone;

  auto flush_includes = [&include_block, &current_block_type, tmp_out, &written,
                         &options]() {
    if (include_block.empty()) return;
    if (ShouldSort(current_block_type, options)) {
      std::sort(include_block.begin(), include_block.end());
    }
    for (const std::string_view line : include_block) {
      written += fwrite(line.data(), 1, line.size(), tmp_out);
    }
    include_block.clear();
    current_block_type = IncludeType::kNone;
  };

  size_t pos = 0;
  while (pos < content.size()) {
    const std::string_view line = ExtractLine(content, pos);
    pos += line.size();

    const IncludeType line_type = GetIncludeType(line);

    if (line_type != IncludeType::kNone) {
      if (!include_block.empty() && line_type != current_block_type) {
        flush_includes();
      }
      include_block.push_back(line);
      current_block_type = line_type;
    } else {
      flush_includes();
      written += fwrite(line.data(), 1, line.size(), tmp_out);
    }
  }
  flush_includes();

  const size_t expected_size = content.size();
  if (written != expected_size) {
    std::cerr << file_to_modify
              << ": Unexpected final size "
                 "original file ("
              << written << " vs. " << expected_size << ")\n";
    fclose(tmp_out);
    unlink(tmp_file_name.c_str());
    return false;
  }
  if (fclose(tmp_out) != 0) {
    unlink(tmp_file_name.c_str());
    return false;
  }

  return rename(tmp_file_name.c_str(), file_to_modify.c_str()) == 0;
}

int main(int argc, char *argv[]) {
  Options options;
  bool s_flag_seen = false;

  int opt;
  while ((opt = getopt(argc, argv, "s:")) != -1) {
    switch (opt) {
      case 's': {
        const std::string_view arg = optarg;
        if (arg == "c") {
          options.sort_c = true;
        } else if (arg == "c++") {
          options.sort_cpp = true;
        } else if (arg == "quote") {
          options.sort_quote = true;
        } else {
          fprintf(stderr, "Unknown header block type for -s: %s\n", optarg);
          return usage(argv[0]);
        }
        s_flag_seen = true;
        break;
      }
      default:
        return usage(argv[0]);
    }
  }

  if (!s_flag_seen) {
    options.sort_c = true;
    options.sort_cpp = true;
    options.sort_quote = true;
  }

  if (optind >= argc) {
    return usage(argv[0]);
  }

  bool success = true;
  for (int i = optind; i < argc; ++i) {
    success &= ModifyFile(argv[i], options);
  }
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
