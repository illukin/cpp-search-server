#pragma once

#include "search_server.h"
#include "document.h"

#include <string>
#include <vector>
#include <deque>

class RequestQueue {
public:
  explicit RequestQueue(const SearchServer &search_server)
    : search_server_(search_server), no_results_requests_(0) {}

  template <typename DocumentPredicate>
  std::vector<Document> AddFindRequest(const std::string& raw_query,
    DocumentPredicate document_predicate) {
    const auto documents = search_server_.FindTopDocuments(raw_query,
      document_predicate);

    RequestQueue::AddRequest(documents.empty());
    return documents;
  }

  std::vector<Document> AddFindRequest
    (const std::string &raw_query, DocumentStatus status);

  std::vector<Document> AddFindRequest(const std::string &raw_query);

  int GetNoResultRequests() const {
    return no_results_requests_;
  }
private:
  void AddRequest(bool is_empty_request);

  const SearchServer &search_server_;

  struct QueryResult {
    bool is_empty;
  };

  std::deque<QueryResult> requests_;
  const static int min_in_day_ = 1440;
  int no_results_requests_;
};
