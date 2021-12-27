#pragma once

#include "document.h"
#include "string_processing.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <vector>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const auto EPSILON = 1e-6;

class SearchServer {
public:
  int GetDocumentId(int index) const;

  template <typename StringContainer>
  explicit SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    using std::string_literals::operator""s;

    for (const auto &word : stop_words_) {
      if (!IsValidWord(word)) {
        throw std::invalid_argument("Special character detected"s);
      }
    }
  }

  explicit SearchServer(const std::string &stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {}

  void AddDocument(int document_id, const std::string &document,
    DocumentStatus status, const std::vector<int> &ratings);

  template <typename Predicate>
  std::vector<Document> FindTopDocuments(const std::string &raw_query,
    Predicate predicate) const;

  std::vector<Document> FindTopDocuments(const std::string &raw_query,
    DocumentStatus requested_status) const;

  std::vector<Document> FindTopDocuments(const std::string &raw_query) const;

  int GetDocumentCount() const;

  std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(
    const std::string &raw_query, int document_id) const;

private:
  struct DocumentData {
    int rating;
    DocumentStatus status;
  };
  struct QueryWord {
    std::string data;
    bool is_minus;
    bool is_stop;
  };
  struct Query {
    std::set<std::string> plus_words;
    std::set<std::string> minus_words;
  };

  std::set<std::string> stop_words_;
  std::map<std::string, std::map<int, double>> word_to_document_freqs_;
  std::map<int, DocumentData> documents_;
  std::vector<int> documents_order_;

  static bool IsValidWord(const std::string &word);
  bool IsStopWord(const std::string &word) const;
  std::vector<std::string> SplitIntoWordsNoStop(const std::string &text) const;
  static int ComputeAverageRating(const std::vector<int> &ratings);
  QueryWord ParseQueryWord(std::string text) const;
  Query ParseQuery(const std::string &text) const;
  double ComputeWordInverseDocumentFreq(const std::string &word) const;
  template <typename Predicate>
  std::vector<Document> FindAllDocuments(const Query& query,
    Predicate predicate) const;
};

template <typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(
  const std::string& raw_query, Predicate predicate) const {
  const Query query = ParseQuery(raw_query);
  auto matched_documents = FindAllDocuments(query, predicate);

  std::sort(matched_documents.begin(), matched_documents.end(),
    [](const Document& lhs, const Document& rhs) {
      if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
        return lhs.rating > rhs.rating;
      } else {
        return lhs.relevance > rhs.relevance;
      }
    });
  if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
    matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
  }
  return matched_documents;
}

template <typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query,
  Predicate predicate) const {
  std::map<int, double> document_to_relevance;
  for (const std::string& word : query.plus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
    const auto docs = word_to_document_freqs_.at(word);
    for (const auto [document_id, term_freq] : docs) {
      const auto &document = documents_.at(document_id);
      if (predicate(document_id, document.status, document.rating)) {
        document_to_relevance[document_id] +=
          term_freq * inverse_document_freq;
      }
    }
  }

  for (const std::string &word : query.minus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
      document_to_relevance.erase(document_id);
    }
  }

  std::vector<Document> matched_documents;
  for (const auto [document_id, relevance] : document_to_relevance) {
    matched_documents.push_back({
      document_id,
      relevance,
      documents_.at(document_id).rating
    });
  }
  return matched_documents;
}
