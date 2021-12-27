#pragma once

#include "search_server.h"

#include <vector>
#include <deque>

class RequestQueue {
public:
  explicit RequestQueue(const SearchServer& search_server)
    : search_server_(&search_server), no_results_requests_(0) {}

  template <typename DocumentPredicate>
  std::vector<Document> AddFindRequest(const std::string &raw_query,
    DocumentPredicate document_predicate);

  std::vector<Document> AddFindRequest
    (const std::string &raw_query, DocumentStatus status);

  std::vector<Document> AddFindRequest(const std::string &raw_query);

  int GetNoResultRequests() const {
    return no_results_requests_;
  }
private:
  const SearchServer *search_server_;

  struct QueryResult {
    std::vector<Document> documents;
    bool is_empty;
  };

  std::deque<QueryResult> requests_;
  const static int min_in_day_ = 1440;
  int no_results_requests_;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query,
  DocumentPredicate document_predicate) {
  // Добавление нового запроса в дек
  QueryResult results;
  results.documents = search_server_->FindTopDocuments(raw_query,
    document_predicate);
  results.is_empty = results.documents.empty();
  requests_.push_back(results);

  // Обновление счётчика пустых запросов в деке
  if (results.is_empty) {
    no_results_requests_++;
  }

  // Удаление из дека устаревших запросов (старше min_in_day_)
  while (requests_.size() > min_in_day_) {

    // Обновление счётчика пустых запросов в деке
    if (requests_.front().is_empty) {
      no_results_requests_--;
    }

    requests_.pop_front();
  }

  return results.documents;
}

std::vector<Document> RequestQueue::AddFindRequest
  (const std::string &raw_query, DocumentStatus status) {
  return AddFindRequest(raw_query,
    [status](int document_id, DocumentStatus document_status, int rating) {
      return document_status == status;
    });
}

std::vector<Document> RequestQueue::AddFindRequest(
  const std::string& raw_query) {
  return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}
