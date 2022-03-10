#include "process_queries.h"

#include <algorithm>
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(
  const SearchServer &search_server,
  const std::vector<std::string> &queries) {
  std::vector<std::vector<Document>> result(queries.size());

  std::transform(std::execution::par, queries.begin(), queries.end(),
    result.begin(), [&search_server](const std::string &query){
    return search_server.FindTopDocuments(query);
  });

  return result;
}

std::vector<Document> ProcessQueriesJoined(
  const SearchServer& search_server,
  const std::vector<std::string>& queries) {
  auto vpq = ProcessQueries(search_server, queries);
  std::vector<Document> result;

  result.reserve(vpq.size());

  for (const auto &vdoc : vpq) {
    for (const auto &doc : vdoc) {
      result.push_back(doc);
    }
  }

  return result;
}
