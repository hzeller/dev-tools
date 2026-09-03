#ifndef HEADER_UTILS_H_
#define HEADER_UTILS_H_

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

inline std::optional<std::string> GetContent(FILE *f) {
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

inline std::optional<std::string> GetContent(const std::string &path) {
  FILE *const file_to_read = fopen(path.c_str(), "rb");
  if (!file_to_read) {
    fprintf(stderr, "%s: can't open: %s\n", path.c_str(), strerror(errno));
    return std::nullopt;
  }
  return GetContent(file_to_read);
}

inline std::string_view ExtractLine(std::string_view content, size_t pos) {
  const size_t newline = content.find('\n', pos);
  if (newline == std::string_view::npos) {
    return content.substr(pos);
  }
  return content.substr(pos, newline - pos + 1);
}

enum class IncludeType {
  kNone,
  kOwnHeader,  // Main implementation header matching the source file name
  kAngleC,      // Standard C headers ending with .h (e.g. <stdio.h>)
  kAngleCpp,    // C++ headers not ending in .h (e.g. <vector>)
  kQuote,
  kOther,
};

inline std::string_view GetStem(std::string_view path) {
  const size_t slash = path.find_last_of("/\\");
  if (slash != std::string_view::npos) {
    path.remove_prefix(slash + 1);
  }
  const size_t dot = path.find_last_of('.');
  if (dot != std::string_view::npos) {
    path = path.substr(0, dot);
  }
  return path;
}

inline std::string_view GetHeaderTarget(std::string_view line) {
  size_t start = line.find_first_of("<\"");
  if (start == std::string_view::npos) return "";
  const char close_char = (line[start] == '<') ? '>' : '"';
  size_t end = line.find(close_char, start + 1);
  if (end == std::string_view::npos) return "";
  return line.substr(start + 1, end - start - 1);
}

inline bool IsSimilarName(std::string_view header_target,
                          std::string_view file_stem) {
  if (file_stem.empty() || header_target.empty()) return false;
  std::string_view header_stem = GetStem(header_target);
  if (header_stem == file_stem) return true;
  if (file_stem.ends_with("_test")) {
    if (header_stem == file_stem.substr(0, file_stem.size() - 5)) return true;
  } else if (file_stem.ends_with("_unittest")) {
    if (header_stem == file_stem.substr(0, file_stem.size() - 9)) return true;
  }
  return false;
}

inline IncludeType GetIncludeType(std::string_view line,
                                  std::string_view file_stem = "",
                                  bool is_first_include = false) {
  while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
    line.remove_prefix(1);
  }
  if (!line.starts_with('#')) return IncludeType::kNone;
  line.remove_prefix(1);
  while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
    line.remove_prefix(1);
  }
  if (!line.starts_with("include")) return IncludeType::kNone;
  line.remove_prefix(7);
  if (line.starts_with("_next")) {
    line.remove_prefix(5);
  }
  while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
    line.remove_prefix(1);
  }
  if (line.empty()) return IncludeType::kOther;

  if (is_first_include && !file_stem.empty()) {
    std::string_view target = GetHeaderTarget(line);
    if (IsSimilarName(target, file_stem)) {
      return IncludeType::kOwnHeader;
    }
  }

  if (line.front() == '<') {
    return line.find(".h>") != std::string_view::npos ? IncludeType::kAngleC
                                                     : IncludeType::kAngleCpp;
  }
  if (line.front() == '"') return IncludeType::kQuote;
  return IncludeType::kOther;
}

inline int GetTypeRank(IncludeType type) {
  switch (type) {
    case IncludeType::kOwnHeader:
      return 0;
    case IncludeType::kAngleC:
      return 1;
    case IncludeType::kAngleCpp:
      return 2;
    case IncludeType::kQuote:
      return 3;
    default:
      return 4;
  }
}

#endif  // HEADER_UTILS_H_
