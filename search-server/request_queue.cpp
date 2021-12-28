#include "request_queue.h"

void RequestQueue::AddRequest(bool is_empty_request) {
  // Добавление нового запроса в дек
  QueryResult results {};
  results.is_empty = is_empty_request;
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
}

std::vector<Document> RequestQueue::AddFindRequest
    (const std::string &raw_query, DocumentStatus status) {
  const auto documents = search_server_.FindTopDocuments(raw_query, status);

  RequestQueue::AddRequest(documents.empty());
  return documents;
}

std::vector<Document> RequestQueue::AddFindRequest(
    const std::string& raw_query) {
  const auto documents = search_server_.FindTopDocuments(raw_query);

  RequestQueue::AddRequest(documents.empty());
  return documents;
}
