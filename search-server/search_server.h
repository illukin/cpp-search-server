#pragma once

#include "concurrent_map.h"
#include "document.h"
#include "string_processing.h"

#include <algorithm>
#include <cmath>
#include <execution>
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
  [[nodiscard]] auto begin() const {
    return documents_ids_.begin();
  }

  [[nodiscard]] auto end() const {
    return documents_ids_.end();
  }

  [[nodiscard]] const std::map<std::string_view, double>
  &GetWordFrequencies(int document_id) const;

  void RemoveDocument(int document_id);

  // Версия метода с возможностью выбора политики выполнения
  template <typename ExecutionPolicy>
  void RemoveDocument(ExecutionPolicy&& policy, int document_id);

  template <typename StringContainer>
  explicit SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    using std::string_literals::operator""s;

    if(!(std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord))) {
      throw std::invalid_argument("Special character detected"s);
    }
  }

  explicit SearchServer(const std::string &stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {}

  explicit SearchServer(const std::string_view &stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {}

  void AddDocument(int document_id, std::string_view document,
    DocumentStatus status, const std::vector<int> &ratings);

  template <typename ExecutionPolicy, typename Predicate>
  std::vector<Document> FindTopDocuments(ExecutionPolicy &&policy,
    std::string_view raw_query, Predicate predicate) const;

  template <typename ExecutionPolicy>
  std::vector<Document> FindTopDocuments(ExecutionPolicy &&policy,
    std::string_view raw_query, DocumentStatus requested_status) const;

  template <typename ExecutionPolicy>
  std::vector<Document> FindTopDocuments(ExecutionPolicy &&policy,
    std::string_view raw_query) const;

  template <typename Predicate>
  std::vector<Document> FindTopDocuments(std::string_view raw_query,
    Predicate predicate) const;

  std::vector<Document> FindTopDocuments(std::string_view raw_query,
    DocumentStatus requested_status) const;

  std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

  int GetDocumentCount() const;

  std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
    std::string_view raw_query, int document_id) const;
  std::tuple<std::vector<std::string_view>, DocumentStatus>
  MatchDocument(const std::execution::sequenced_policy&,
    std::string_view raw_query,int document_id) const;
  std::tuple<std::vector<std::string_view>, DocumentStatus>
  MatchDocument(const std::execution::parallel_policy&,
    std::string_view raw_query,int document_id) const;

private:
  struct DocumentData {
    int rating;
    DocumentStatus status;
    std::string data;
  };

  struct QueryWord {
    std::string_view data;
    bool is_minus;
    bool is_stop;
  };

  struct Query {
    std::vector<std::string_view> plus_words;
    std::vector<std::string_view> minus_words;
  };

  std::set<std::string> stop_words_;
  std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
  std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
  std::map<int, DocumentData> documents_;
  std::set<int> documents_ids_;

  static bool IsValidWord(std::string_view word);
  bool IsStopWord(std::string_view word) const;
  std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;
  static int ComputeAverageRating(const std::vector<int> &ratings);
  QueryWord ParseQueryWord(std::string_view text) const;
  Query ParseQuery(std::string_view text, bool=true) const;
  double ComputeWordInverseDocumentFreq(std::string_view word) const;

  template <typename ExecutionPolicy, typename Predicate>
  std::vector<Document> FindAllDocuments(ExecutionPolicy &&policy,
    const Query& query, Predicate predicate) const;
};

template <typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
  if (document_to_word_freqs_.count(document_id) == 0) {
    return;
  }

  const auto &word_freq = document_to_word_freqs_.at(document_id);
  std::vector<std::string_view> words(word_freq.size());

  std::transform(policy, word_freq.begin(), word_freq.end(), words.begin(),
    [](const std::pair<const std::string_view, double> &el){
      return el.first;
    });

  std::for_each(policy, words.begin(), words.end(),
    [this, document_id](const std::string_view key){
      word_to_document_freqs_.at(key).erase(document_id);
    });

  document_to_word_freqs_.erase(document_id);
  documents_.erase(document_id);
  documents_ids_.erase(document_id);
}

template <typename ExecutionPolicy, typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy &&policy,
  const std::string_view raw_query, Predicate predicate) const {
  const Query query = ParseQuery(raw_query);
  auto matched_documents = FindAllDocuments(policy, query, predicate);
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

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy &&policy,
  const std::string_view raw_query, DocumentStatus requested_status) const {
  return SearchServer::FindTopDocuments(policy, raw_query,
    [requested_status](int document_id,
      DocumentStatus status, int rating) {
      return requested_status == status;
    });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy &&policy,
  const std::string_view raw_query) const {
  return SearchServer::FindTopDocuments(policy, raw_query,
    DocumentStatus::ACTUAL);
}

template <typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(
  const std::string_view raw_query, Predicate predicate) const {
  return SearchServer::FindTopDocuments(std::execution::seq, raw_query,
    predicate);
}

template <typename ExecutionPolicy, typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy &&policy,
  const Query& query, Predicate predicate) const {
  ConcurrentMap<int, double> document_to_relevance(5000);

  std::for_each(policy, query.plus_words.begin(), query.plus_words.end(),
    [this, predicate, &document_to_relevance](std::string_view word) {
      if (word_to_document_freqs_.count(word) == 0) {
        return;
      }

      const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
      const auto docs = word_to_document_freqs_.at(word);

      for (const auto [document_id, term_freq] : docs) {
        const auto &document = documents_.at(document_id);
        if (predicate(document_id, document.status, document.rating)) {
          document_to_relevance[document_id].ref_to_value +=
            term_freq * inverse_document_freq;
        }
      }
    }
  );

  std::for_each(policy, query.minus_words.begin(), query.minus_words.end(),
    [this, &document_to_relevance](std::string_view word) {
      if (word_to_document_freqs_.count(word) == 0) {
        return;
      }
      for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
        document_to_relevance.Erase(document_id);
      }
    }
  );

  std::vector<Document> matched_documents;
  for (const auto [document_id, relevance]
    : document_to_relevance.BuildOrdinaryMap()) {
    matched_documents.emplace_back(
      document_id,
      relevance,
      documents_.at(document_id).rating
    );
  }
  return matched_documents;
}
