#pragma once

#include <set>
#include <string>
#include <vector>

std::vector<std::string_view> SplitIntoWords(std::string_view text);

// Возвращает множество уникальных НЕ пустых строк
template <typename StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(
  const StringContainer &strings) {
  std::set<std::string> non_empty_strings;

  for (const std::string_view str : strings)
    if (!str.empty()) {
      non_empty_strings.insert(std::string(str));
    }

  return non_empty_strings;
}
