#include "remove_duplicates.h"

#include <map>
#include <set>

void RemoveDuplicates(SearchServer& search_server) {
  std::map<int, std::set<std::string>> result;

  for (const auto &document_id : search_server) {
    auto temp = search_server.GetWordFrequencies(document_id);

    for (const auto &[key, val] : temp) {
      result[document_id].insert(key);
    }
  }

  for (const auto &[key, val] : result) {
    for (const auto &[key2, val2] : result) {
      if (key < key2 && val == val2) {
        search_server.RemoveDocument(key2);
        std::cout << "Found duplicate document id " << key2 << std::endl;
      }
    }
  }
}
