#include "search_server.h"

#include <execution>

using std::string_literals::operator""s;

const std::map<std::string_view, double>
&SearchServer::GetWordFrequencies(int document_id) const {
  static std::map<std::string_view, double> result;

  if (document_to_word_freqs_.count(document_id) != 0) {
    for (const auto &[key, val] : document_to_word_freqs_.at(document_id)) {
      result[key] = val;
    }
  }

  return result;
}

void SearchServer::RemoveDocument(int document_id) {
  return RemoveDocument(std::execution::seq, document_id);
}

void SearchServer::AddDocument(int document_id, std::string_view document,
  DocumentStatus status, const std::vector<int> &ratings) {

  // Попытка добавить документ с отрицательным id или с id ранее добавленного
  // документа
  if ((document_id < 0) || (documents_.count(document_id) > 0)) {
    throw std::invalid_argument("Invalid document id"s);
  }

  documents_.emplace(document_id,
    DocumentData{
      ComputeAverageRating(ratings),
      status,
      std::string(document) // Оригинал строки
    });

  auto &src_string = documents_.find(document_id)->second.data;
  const std::vector<std::string_view> words = SplitIntoWordsNoStop(src_string);
  const double inv_word_count = 1.0 / words.size();

  for (const std::string_view word : words) {
    word_to_document_freqs_[word][document_id] += inv_word_count;
    document_to_word_freqs_[document_id][word] += inv_word_count;
  }

  documents_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(
  const std::string_view raw_query, DocumentStatus requested_status) const {
  return SearchServer::FindTopDocuments(raw_query,
    [requested_status](int document_id,
      DocumentStatus status, int rating) {
      return requested_status == status;
    });
}

std::vector<Document> SearchServer::FindTopDocuments(
  const std::string_view raw_query) const {
  return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
  return static_cast<int>(documents_.size());
}

std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
  return MatchDocument(std::execution::seq, raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(const std::execution::sequenced_policy&,
  const std::string_view raw_query, int document_id) const {
  const Query query = ParseQuery(raw_query);
  const auto status = documents_.at(document_id).status;

  for (const std::string_view word : query.minus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    if (word_to_document_freqs_.at(word).count(document_id) > 0) {
      return {std::vector<std::string_view>(), status};
    }
  }

  std::vector<std::string_view> matched_words;
  for (const std::string_view word : query.plus_words) {
    if (word_to_document_freqs_.count(word) == 0) {
      continue;
    }
    if (word_to_document_freqs_.at(word).count(document_id) > 0) {
      matched_words.push_back(word);
    }
  }

  return {matched_words, status};
}

std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(const std::execution::parallel_policy&,
  std::string_view raw_query, int document_id) const {

  const auto query = ParseQuery(raw_query, false);
  const auto status = documents_.at(document_id).status;
  const auto word_checker =
    [this, document_id](std::string_view word) {
      const auto it = word_to_document_freqs_.find(word);
      return it != word_to_document_freqs_.end() && it->second.count(document_id);
    };

  if (any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), word_checker)) {
    return {std::vector<std::string_view>(), status};
  }

  std::vector<std::string_view> matched_words(query.plus_words.size());
  auto words_end = copy_if(
    std::execution::par,
    query.plus_words.begin(), query.plus_words.end(),
    matched_words.begin(),
    word_checker
  );

  sort(matched_words.begin(), words_end);
  words_end = unique(matched_words.begin(), words_end);
  matched_words.erase(words_end, matched_words.end());

  return {matched_words, status};
}

bool SearchServer::IsValidWord(const std::string_view word) {
  return std::none_of(word.begin(), word.end(), [](char c) {
    return c >= '\0' && c < ' ';
  });
}

bool SearchServer::IsStopWord(const std::string_view word) const {
  return stop_words_.count(std::string(word)) > 0;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(
  const std::string_view text) const {
  std::vector<std::string_view> words;

  for (const std::string_view word : SplitIntoWords(text)) {
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

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
  bool is_minus = false;

  if (text.empty()) {
    throw std::invalid_argument("Word is empty"s);
  }

  // Удаление лидирующего минуса
  if (text.front() == '-') {
    text.remove_prefix(1);
    is_minus = true;
  }

  // Проверка на пустоту после удаления лидирующего минуса (отсутствие в
  // поисковом запросе текста после символа «минус»)
  if (text.empty()) {
    throw std::invalid_argument("No text after minus"s);
  }

  // Указание в поисковом запросе более чем одного минуса перед словами,
  // которых не должно быть в документах
  if (text.front() == '-') {
    throw std::invalid_argument("Double minus"s);
  }

  // Проверка на запрещённые символы
  if (!IsValidWord(text)) {
    throw std::invalid_argument("Special character detected"s);
  }

  bool is_stop_word = IsStopWord(text);

  return {
    text,
    is_minus,
    is_stop_word
  };
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view text,
  bool make_uniq) const {
  Query query;

  for (const std::string_view word : SplitIntoWords(text)) {
    const auto query_word = ParseQueryWord(word);

    if (!query_word.is_stop) {
      if (query_word.is_minus) {
        query.minus_words.push_back(query_word.data);
      } else {
        query.plus_words.push_back(query_word.data);
      }
    }
  }

  if (make_uniq) {
    // Удаление дубликатов из векторов "плюс" и "минус" слов
    for (auto *word : {&query.plus_words, &query.minus_words}) {
      std::sort(word->begin(), word->end());
      auto trash_pos = std::unique(word->begin(), word->end());
      word->erase(trash_pos, word->end());
    }
  }

  return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(
  const std::string_view word) const {
  const auto size = word_to_document_freqs_.at(word).size();
  return std::log(GetDocumentCount() * 1.0 / size);
}
