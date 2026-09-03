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

#include <algorithm>
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
#include <vector>

#include "header-utils.h"

namespace {
struct Options {
  // Quiet operation. Don't print informational messages.
  bool quiet = false;

  // Explanation to write on stderr if header added.
  std::string explanation;

  // Alignment for informational messages
  int print_alignment = 40;

  // Starting point for writing headers, if found.
  std::string insert_marker;
};
}  // namespace

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
          "\t-m<write-marker>: Add headers after this marker text if available\n"
          "\t-a<width>       : When printing message: align filename width\n",
          progname, progname);
  return EXIT_FAILURE;
}

static bool ModifyFile(const std::string &file_to_modify,
                       const std::string &insert_header,
                       const Options &options) {
  auto content_or = GetContent(file_to_modify);
  if (!content_or.has_value()) {
    return false;
  }
  const std::string &content = *content_or;

  const std::string report_filename = file_to_modify + ":";
  if (content.find(insert_header) != std::string::npos) {
    if (!options.quiet) {
      fprintf(stderr, "%*s %s already there\n", -options.print_alignment,
              report_filename.c_str(), insert_header.c_str());
    }
    return true;
  }

  if (!options.explanation.empty()) {
    fprintf(stdout, "%*s %s %s\n", -options.print_alignment, report_filename.c_str(),
            insert_header.c_str(), options.explanation.c_str());
  }

  size_t insert_mark = 0;
  if (!options.insert_marker.empty()) {
    if (auto m = content.find(options.insert_marker); m != std::string::npos) {
      insert_mark = m + options.insert_marker.length();
      // Insert after the line matching this
      while (insert_mark < content.length() && content[insert_mark] != '\n') {
        ++insert_mark;
      }
      if (insert_mark < content.length()) {
        ++insert_mark;
      }
    }
  }

  const std::string_view file_stem = GetStem(file_to_modify);
  const std::string line_to_insert = insert_header + "\n";
  const IncludeType target_type = GetIncludeType(line_to_insert, file_stem, true);

  bool matching_block_exists = false;
  bool higher_rank_block_exists = false;
  bool any_include_exists_in_file = false;
  bool first_include_seen = false;
  size_t scan_pos = insert_mark;
  while (scan_pos < content.size()) {
    const std::string_view line = ExtractLine(content, scan_pos);
    scan_pos += line.size();
    const bool is_first = !first_include_seen;
    const IncludeType lt = GetIncludeType(line, file_stem, is_first);
    if (lt != IncludeType::kNone) {
      first_include_seen = true;
      any_include_exists_in_file = true;
      if (lt == target_type) {
        matching_block_exists = true;
      }
      if (GetTypeRank(lt) > GetTypeRank(target_type)) {
        higher_rank_block_exists = true;
      }
    }
  }

  const std::string tmp_file_name = file_to_modify + ".tmp";
  FILE *const tmp_out = fopen(tmp_file_name.c_str(), "wb");
  if (!tmp_out) {
    fprintf(stderr, "%s: can't open tmp file %s: %s\n", file_to_modify.c_str(),
            tmp_file_name.c_str(), strerror(errno));
    return false;
  }

  size_t written = 0;
  if (insert_mark > 0) {
    written += fwrite(content.data(), 1, insert_mark, tmp_out);
  }

  std::vector<std::string_view> include_block;
  IncludeType current_block_type = IncludeType::kNone;
  bool inserted = false;
  bool target_block_modified = false;
  bool last_flushed_had_includes = false;

  auto flush_includes = [&include_block, &current_block_type,
                         &target_block_modified, &last_flushed_had_includes,
                         tmp_out, &written]() {
    if (include_block.empty()) return;
    if (target_block_modified) {
      std::sort(include_block.begin(), include_block.end());
      target_block_modified = false;
    }
    for (const std::string_view line : include_block) {
      written += fwrite(line.data(), 1, line.size(), tmp_out);
    }
    include_block.clear();
    current_block_type = IncludeType::kNone;
    last_flushed_had_includes = true;
  };

  auto emit_new_target_block = [&](bool followed_by_include) {
    if (inserted) return;
    if (last_flushed_had_includes || !include_block.empty()) {
      written += fwrite("\n", 1, 1, tmp_out);
    }
    written += fwrite(line_to_insert.data(), 1, line_to_insert.size(), tmp_out);
    if (followed_by_include) {
      written += fwrite("\n", 1, 1, tmp_out);
    }
    inserted = true;
    last_flushed_had_includes = true;
  };

  first_include_seen = false;
  size_t pos = insert_mark;
  while (pos < content.size()) {
    const std::string_view line = ExtractLine(content, pos);
    pos += line.size();

    const bool is_first = !first_include_seen;
    const IncludeType line_type = GetIncludeType(line, file_stem, is_first);

    if (line_type != IncludeType::kNone) {
      first_include_seen = true;
      if (!include_block.empty() && line_type != current_block_type) {
        flush_includes();
      }

      if (!inserted) {
        if (matching_block_exists) {
          if (line_type == target_type) {
            include_block.push_back(line_to_insert);
            inserted = true;
            target_block_modified = true;
          }
        } else if (higher_rank_block_exists) {
          if (GetTypeRank(line_type) > GetTypeRank(target_type)) {
            emit_new_target_block(/*followed_by_include=*/true);
          }
        }
      }

      include_block.push_back(line);
      current_block_type = line_type;
    } else {
      if (!inserted && !matching_block_exists && !higher_rank_block_exists) {
        if (any_include_exists_in_file) {
          if (first_include_seen && !IsBlankLine(line)) {
            flush_includes();
            emit_new_target_block(/*followed_by_include=*/false);
            written += fwrite("\n", 1, 1, tmp_out);
          }
        } else if (!IsBlankLine(line)) {
          fseek(tmp_out, insert_mark, SEEK_SET);
          written = insert_mark;
          written += fwrite(line_to_insert.data(), 1, line_to_insert.size(), tmp_out);
          written += fwrite("\n", 1, 1, tmp_out);
          inserted = true;
        }
      }
      flush_includes();
      written += fwrite(line.data(), 1, line.size(), tmp_out);
      if (IsBlankLine(line)) {
        last_flushed_had_includes = false;
      }
    }
  }

  if (!inserted) {
    if (!any_include_exists_in_file) {
      fseek(tmp_out, insert_mark, SEEK_SET);
      written = insert_mark;
      written += fwrite(line_to_insert.data(), 1, line_to_insert.size(), tmp_out);
      inserted = true;
    } else {
      flush_includes();
      emit_new_target_block(/*followed_by_include=*/false);
    }
  } else {
    flush_includes();
  }

  if (fclose(tmp_out) != 0) {
    unlink(tmp_file_name.c_str());
    return false;
  }

  return rename(tmp_file_name.c_str(), file_to_modify.c_str()) == 0;
}

int main(int argc, char *argv[]) {
  Options options;
  int opt;
  while ((opt = getopt(argc, argv, "qe:a:m:")) != -1) {
    switch (opt) {
      case 'q':
        options.quiet = true;
        break;
      case 'e':
        options.explanation = optarg;
        break;
      case 'a':
        options.print_alignment = atoi(optarg);
        break;
      case 'm':
        options.insert_marker = optarg;
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

  if (start_files >= argc && !options.quiet) {
    std::cerr << "No files provided\n";
  }

  bool success = true;
  for (int i = start_files; i < argc; ++i) {
    success &= ModifyFile(argv[i], insert_header, options);
  }
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
