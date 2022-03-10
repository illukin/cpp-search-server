#include "remove_duplicates.h"

#include <map>
#include <set>

void RemoveDuplicates(SearchServer& search_server) {
  std::map<int, std::set<std::string_view>> result;
  std::set<std::set<std::string_view>> set_of_sets;

  for (const auto &document_id : search_server) {
    auto temp = search_server.GetWordFrequencies(document_id);

    for (const auto &[key, val] : temp) {
      result[document_id].insert(key);
    }
  }

  for (const auto &[key, val] : result) {
    auto [unused , is_inserted] = set_of_sets.insert(val);

    if (!is_inserted) {
      search_server.RemoveDocument(key);
      std::cout << "Found duplicate document id " << key << std::endl;
    }
  }
}
