#include "search_server.h"

using std::string_literals::operator""s;

int SearchServer::GetDocumentId(int index) const {
  return documents_order_.at(index);
}

void SearchServer::AddDocument(int document_id, const std::string &document,
  DocumentStatus status, const std::vector<int> &ratings) {

  // Попытка добавить документ с отрицательным id или с id ранее добавленного
  // документа
  if ((document_id < 0) || (documents_.count(document_id) > 0)) {
    throw std::invalid_argument("Invalid document id"s);
  }

  const std::vector<std::string> words = SplitIntoWordsNoStop(document);
  const double inv_word_count = 1.0 / words.size();
  for (const std::string &word : words) {
    word_to_document_freqs_[word][document_id] += inv_word_count;
  }
  documents_.emplace(document_id,
    DocumentData{
      ComputeAverageRating(ratings),
      status
    });
  documents_order_.push_back(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(
  const std::string &raw_query, DocumentStatus requested_status) const {
  return SearchServer::FindTopDocuments(raw_query,
    [requested_status](int document_id,
      DocumentStatus status, int rating) {
      return requested_status == status;
    });
}

std::vector<Document> SearchServer::FindTopDocuments(
  const std::string& raw_query) const {
  return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
  return static_cast<int>(documents_.size());
}

std::tuple<std::vector<std::string>, DocumentStatus>
  SearchServer::MatchDocument(const std::string &raw_query,
    int document_id) const {
  const Query query = ParseQuery(raw_query);
  std::vector<std::string> matched_words;
  for (const std::string &word : query.plus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    if (word_to_document_freqs_.at(word).count(document_id) > 0) {
      matched_words.push_back(word);
    }
  }
  for (const std::string &word : query.minus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    if (word_to_document_freqs_.at(word).count(document_id) > 0) {
      matched_words.clear();
      break;
    }
  }
  return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsValidWord(const std::string &word) {
  return none_of(word.begin(), word.end(), [](char c) {
    return c >= '\0' && c < ' ';
  });
}

bool SearchServer::IsStopWord(const std::string &word) const {
  return stop_words_.count(word) > 0;
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(
  const std::string& text) const {
  std::vector<std::string> words;
  for (const std::string &word : SplitIntoWords(text)) {
    if (!IsValidWord(word)) {
      throw std::invalid_argument("Special character detected"s);
    }

    if (!IsStopWord(word)) {
      words.push_back(word);
    }
  }
  return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
  if (ratings.empty()) {
    return 0;
  }

  int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);

  return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
  bool is_minus = false;

  if (text.empty()) {
    throw std::invalid_argument("Word is empty"s);
  }

  // Удаление лидирующего минуса
  if (text[0] == '-') {
    text.erase(0, 1);
    is_minus = true;
  }

  // Проверка на пустоту после удаления лидирующего минуса (отсутствие в
  // поисковом запросе текста после символа «минус»)
  if (text.empty()) {
    throw std::invalid_argument("No text after minus"s);
  }

  // Указание в поисковом запросе более чем одного минуса перед словами,
  // которых не должно быть в документах
  if (text[0] == '-') {
    throw std::invalid_argument("Double minus"s);
  }

  // Проверка на запрещённые символы
  if (!IsValidWord(text)) {
    throw std::invalid_argument("Special character detected"s);
  }

  return {
    text,
    is_minus,
    IsStopWord(text)
  };
}

SearchServer::Query SearchServer::ParseQuery(const std::string &text) const {
  Query query;
  for (const std::string &word : SplitIntoWords(text)) {
    const QueryWord query_word = ParseQueryWord(word);
    if (!query_word.is_stop) {
      if (query_word.is_minus) {
        query.minus_words.insert(query_word.data);
      } else {
        query.plus_words.insert(query_word.data);
      }
    }
  }
  return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(
  const std::string& word) const {
  const auto size = word_to_document_freqs_.at(word).size();
  return std::log(GetDocumentCount() * 1.0 / size);
}
