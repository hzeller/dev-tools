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
  kAngleC,    // Standard C headers ending with .h (e.g. <stdio.h>)
  kAngleCpp,  // C++ headers not ending in .h (e.g. <vector>)
  kQuote,
  kOther,
};

inline IncludeType GetIncludeType(std::string_view line) {
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
  if (line.front() == '<') {
    return line.find(".h>") != std::string_view::npos ? IncludeType::kAngleC
                                                     : IncludeType::kAngleCpp;
  }
  if (line.front() == '"') return IncludeType::kQuote;
  return IncludeType::kOther;
}

inline int GetTypeRank(IncludeType type) {
  switch (type) {
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
