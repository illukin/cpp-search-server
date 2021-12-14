// search_server_s1_t2_v2.cpp

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const auto EPSILON = 1e-6;

string ReadLine() {
  string s;
  getline(cin, s);
  return s;
}

int ReadLineWithNumber() {
  int result;
  cin >> result;
  ReadLine();
  return result;
}

// Проверка символа (запрещены символы с кодами от 0 до 31)
static bool isValidCharacter(char c) {
  return c >= '\0' && c < ' ';
}

// Проверка строки на запрещённые символы
static bool IsValidWord(const string& word) {
  return none_of(word.begin(), word.end(), [](char c) {
    return isValidCharacter(c);
  });
}

vector<string> SplitIntoWords(const string& text) {
  vector<string> words;
  string word;
  for (const char c : text) {
    if (c == ' ') {
      if (!word.empty()) {
        words.push_back(word);
        word.clear();
      }
    } else {
      if (isValidCharacter(c))
        throw invalid_argument("Special character detected");

      word += c;
    }
  }
  if (!word.empty()) {
    words.push_back(word);
  }

  return words;
}

struct Document {
  Document() = default;

  Document(int id, double relevance, int rating)
    : id(id)
    , relevance(relevance)
    , rating(rating) {
  }

  int id = 0;
  double relevance = 0.0;
  int rating = 0;
};

enum class DocumentStatus {
  ACTUAL,
  IRRELEVANT,
  BANNED,
  REMOVED,
};

// Возвращает множество уникальных НЕ пустых строк
template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
  set<string> non_empty_strings;

  for (const string& str : strings)
    if (!str.empty()) {
      if (!IsValidWord(str))
        throw invalid_argument("Special character detected");

      non_empty_strings.insert(str);
    }

  return non_empty_strings;
}

class SearchServer {
public:
  int GetDocumentId(int index) const {
    return documents_order_.at(index);
  }

  template <typename StringContainer>
  explicit SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {}

  explicit SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text)) {}

  void AddDocument(int document_id, const string& document,
                   DocumentStatus status, const vector<int>& ratings) {

    // Попытка добавить документ с отрицательным id или с id ранее добавленного
    // документа
    if ((document_id < 0) || (documents_.count(document_id) > 0))
      throw invalid_argument("Invalid document id");

    const vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / (double)words.size();
    for (const string& word : words) {
      word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    documents_.emplace(document_id,
                       DocumentData{
                           ComputeAverageRating(ratings),
                           status
                       });
    documents_order_.push_back(document_id);
  }

  template <typename Predicate>
  vector<Document> FindTopDocuments(const string& raw_query,
                                    Predicate predicate) const {
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, predicate);

    sort(matched_documents.begin(), matched_documents.end(),
         [](const Document& lhs, const Document& rhs) {
           if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
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

  vector<Document> FindTopDocuments(const string& raw_query,
                                    DocumentStatus requested_status) const {
    return FindTopDocuments(raw_query,
                            [requested_status](int document_id,
                                DocumentStatus status, int rating) {
      return requested_status == status;
    });
  }

  vector<Document> FindTopDocuments(const string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
  }

  int GetDocumentCount() const {
    return (int)documents_.size();
  }

  tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                      int document_id) const {
    const Query query = ParseQuery(raw_query);
    vector<string> matched_words;
    for (const string& word : query.plus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      if (word_to_document_freqs_.at(word).count(document_id)) {
        matched_words.push_back(word);
      }
    }
    for (const string& word : query.minus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      if (word_to_document_freqs_.at(word).count(document_id)) {
        matched_words.clear();
        break;
      }
    }
    return {matched_words, documents_.at(document_id).status};
  }

private:
  struct DocumentData {
    int rating;
    DocumentStatus status;
  };

  set<string> stop_words_;
  map<string, map<int, double>> word_to_document_freqs_;
  map<int, DocumentData> documents_;
  vector<int> documents_order_;

  bool IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
  }

  vector<string> SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
      if (!IsStopWord(word)) {
        words.push_back(word);
      }
    }
    return words;
  }

  static int ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
      return 0;
    }

    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);

    return rating_sum / static_cast<int>(ratings.size());
  }

  struct QueryWord {
    string data;
    bool is_minus;
    bool is_stop;
  };

  QueryWord ParseQueryWord(string text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {

      // Указание в поисковом запросе более чем одного минуса перед словами,
      // которых не должно быть в документах
      if (text[1] == '-')
        throw invalid_argument("Double minus");

      // Отсутствие в поисковом запросе текста после символа «минус»
      if (text.size() == 1u)
        throw invalid_argument("No text after minus");

      is_minus = true;
      text = text.substr(1);
    }
    return {
        text,
        is_minus,
        IsStopWord(text)
    };
  }

  struct Query {
    set<string> plus_words;
    set<string> minus_words;
  };

  Query ParseQuery(const string& text) const {
    Query query;
    for (const string& word : SplitIntoWords(text)) {
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

  // Existence required
  double ComputeWordInverseDocumentFreq(const string& word) const {
    const auto size = word_to_document_freqs_.at(word).size();
    return log(GetDocumentCount() * 1.0 / (double)size);
  }

  template <typename Predicate>
  vector<Document> FindAllDocuments(const Query& query,
                                    Predicate predicate) const {
    map<int, double> document_to_relevance;
    for (const string& word : query.plus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
      const auto docs = word_to_document_freqs_.at(word);
      for (const auto [document_id, term_freq] : docs) {
        auto document = documents_.at(document_id);
        if (predicate(document_id, document.status, document.rating)) {
          document_to_relevance[document_id] +=
              term_freq * inverse_document_freq;
        }
      }
    }

    for (const string& word : query.minus_words) {
      if (word_to_document_freqs_.count(word) == 0) {
        continue;
      }
      for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
        document_to_relevance.erase(document_id);
      }
    }

    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
      matched_documents.push_back({
                                      document_id,
                                      relevance,
                                      documents_.at(document_id).rating
                                  });
    }
    return matched_documents;
  }
};

void AssertImpl(bool value, const string& expr_str, const string& file,
                const string& func, unsigned line, const string& hint) {
  if (!value) {
    cout << file << "("s << line << "): "s << func << ": "s;
    cout << "ASSERT("s << expr_str << ") failed."s;
    if (!hint.empty()) {
      cout << " Hint: "s << hint;
    }
    cout << endl;
    abort();
  }
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str,
                     const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
  if (t != u) {
    cout << boolalpha;
    cout << file << "("s << line << "): "s << func << ": "s;
    cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
    cout << t << " != "s << u << "."s;
    if (!hint.empty()) {
      cout << " Hint: "s << hint;
    }
    cout << endl;
    abort();
  }
}

template <typename FUNC>
void RunTestImpl(const FUNC &run_test, string func_name) {
  run_test();
  cerr << func_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, \
__FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, \
__FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, \
__LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, \
__FUNCTION__, __LINE__, (hint))

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении
// документов
void TestExcludeStopWordsFromAddedDocumentContent() {
  const int doc_id = 42;
  const string content = "cat in the city"s;
  const vector<int> ratings = {1, 2, 3};
  {
    SearchServer server(""s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments("in"s);
    ASSERT_EQUAL(found_docs.size(), 1u);
    const Document& doc0 = found_docs[0];
    ASSERT_EQUAL(doc0.id, doc_id);
  }

  {
    SearchServer server("in the"s);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                "Stop words must be excluded from documents"s);
  }
}

// Проверка поддержки минус-слов. Документы, содержащие минус-слова поискового
// запроса, не должны включаться в результаты поиска
void TestMinusWords() {
  SearchServer ss(""s);
  const auto query = "ухоженный -кот"s;

  // Два следующих документа должны быть исключены из результатов запроса
  ss.AddDocument(0, "белый кот и модный ошейник"s,
                 DocumentStatus::ACTUAL, {8, -3});
  ss.AddDocument(1, "пушистый кот пушистый хвост"s,
                 DocumentStatus::ACTUAL, {7, 2, 7});

  // Единственный документ, который должен будет вернуться в результате запроса
  ss.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                 DocumentStatus::ACTUAL, {5, -12, 2, 1});

  const auto found_docs = ss.FindTopDocuments(query);
  ASSERT(found_docs.size() == 1u);
  ASSERT(found_docs[0].id == 2);
}

// Тест матчинга документов. При матчинге документа по поисковому запросу должны
// быть возвращены все слова из поискового запроса, присутствующие в документе.
// Если есть соответствие хотя бы по одному минус-слову, должен возвращаться
// пустой список слов.
void TestMatchingDocuments() {
  SearchServer ss(""s);

  ss.AddDocument(0, "белый кот и модный ошейник"s,
                 DocumentStatus::ACTUAL, {8, -3});
  ss.AddDocument(1, "пушистый кот пушистый хвост"s,
                 DocumentStatus::ACTUAL, {7, 2, 7});
  ss.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                 DocumentStatus::ACTUAL, {5, -12, 2, 1});

  // Должны быть возвращены все слова из поискового запроса, присутствующие в
  // документе
  {
    const auto [words, status] = ss.MatchDocument("пушистый кот"s, 1);
    vector<string> query_w = {"пушистый"s, "кот"s};
    ASSERT_EQUAL(words.size(), query_w.size());

    // Вектор слов результата должен содержать только те же слова, и в том
    // же количестве, что и вектор слов запроса
    for (const auto &word : words)
      ASSERT(count(query_w.begin(), query_w.end(), word) == 1);
  }

  // Если есть соответствие хотя бы по одному минус-слову, должен возвращаться
  // пустой список слов
  {
    const auto [words, status] = ss.MatchDocument("пушистый -кот"s, 1);
    ASSERT_HINT(words.empty(),
                "Minus-word was detected, but result doesn't empty"s);
  }
}

// Сортировка найденных документов по релевантности. Возвращаемые при поиске
// документов результаты должны быть отсортированы в порядке убывания
// релевантности
void TestSortRelevance() {
  SearchServer ss(""s);

  ss.AddDocument(0, "белый кот и модный ошейник"s,
                 DocumentStatus::ACTUAL, {8, -3});
  ss.AddDocument(1, "пушистый кот пушистый хвост"s,
                 DocumentStatus::ACTUAL, {7, 2, 7});
  ss.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                 DocumentStatus::ACTUAL, {5, -12, 2, 1});

  auto found_docs = ss.FindTopDocuments("пушистый ухоженный кот");

  ASSERT_HINT(found_docs[0].id == 1
    && found_docs[1].id == 2
    && found_docs[2].id == 0,
    "Incorrect sort by relevance"s);
}

// Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему
// арифметическому оценок документа.
void TestCalcRating() {
  SearchServer ss(""s);
  vector<int> ratings = {5, -12, 2, 1};
  int average_rating = std::accumulate(ratings.begin(), ratings.end(), 0)
      / (int)ratings.size();

  ss.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                 DocumentStatus::ACTUAL, ratings);

  auto found_docs = ss.FindTopDocuments("ухоженный"s);
  ASSERT(found_docs.size() == 1u);

  const Document &doc0 = found_docs[0];
  ASSERT_EQUAL_HINT(doc0.rating, average_rating,
                    "Incorrect document average rating");
}

// Фильтрация результатов поиска с использованием предиката, задаваемого
// пользователем
void TestFilterPredicate() {
  SearchServer ss(""s);

  // Документы с рейтингом больше нуля
  auto predicate = [](int id, DocumentStatus status, int rating){
    return rating > 0;
  };

  ss.AddDocument(0, "белый кот и модный ошейник"s,
                 DocumentStatus::ACTUAL, {8, -3});
  ss.AddDocument(1, "пушистый кот пушистый хвост"s,
                 DocumentStatus::BANNED, {7, 2, 7});
  ss.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                 DocumentStatus::ACTUAL, {5, -12, 2, 1});

  auto found_docs = ss.FindTopDocuments("кот"s, predicate);
  ASSERT(found_docs.size() == 2u);

  // Создание двух векторов с id и рейтингом документов для дальнейших
  // проверок этих значений
  vector<int> ids, ratings;
  for (const auto &document : found_docs) {
    ids.push_back(document.id);
    ratings.push_back(document.rating);
  }

  // В результате должны быть только документы с id = 1 и id = 2
  ASSERT_HINT(count(ids.begin(), ids.end(), 0) == 1
              && count(ids.begin(), ids.end(), 1) == 1,
              "Incorrect document ids"s);

  // В результате должны быть документы с рейтингом больше нуля
  for (const auto &rating : ratings)
    ASSERT_HINT(rating > 0, "Incorrect document rating"s);
}

// Поиск документов, имеющих заданный статус
void TestSearchDocumentsWithStatus() {
  SearchServer ss(""s);

  ss.AddDocument(0, "белый кот и модный ошейник"s,
                 DocumentStatus::ACTUAL, {8, -3});
  ss.AddDocument(1, "пушистый кот пушистый хвост"s,
                 DocumentStatus::BANNED, {7, 2, 7});
  ss.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                 DocumentStatus::ACTUAL, {5, -12, 2, 1});

  auto found_docs = ss.FindTopDocuments("кот"s,
                                        DocumentStatus::BANNED);
  ASSERT(found_docs.size() == 1u);
  ASSERT(found_docs[0].id == 1);
}

// Вспомогательная функция для теста TestCalcRelevance()
vector<string> SplitIntoWordsNoStop(const string& text,
                                    const vector<string> &stop_words) {
  vector<string> words;
  for (const string& word : SplitIntoWords(text)) {
    if (count(stop_words.begin(), stop_words.end(), word) == 0) {
      words.push_back(word);
    }
  }
  return words;
}

// Корректное вычисление релевантности найденных документов
void TestCalcRelevance() {
  auto stop_words = "и в на"s;
  SearchServer ss(stop_words);

  auto stop_words_v = SplitIntoWords(stop_words);
  auto doc1 = "белый кот и модный ошейник"s;
  auto doc2 = "пушистый кот пушистый хвост"s;
  auto doc3 = "ухоженный пёс выразительные глаза"s;

  ss.AddDocument(0, doc1, DocumentStatus::ACTUAL, {8, -3});
  ss.AddDocument(1, doc2, DocumentStatus::ACTUAL, {7, 2, 7});
  ss.AddDocument(2, doc3, DocumentStatus::ACTUAL, {5, -12, 2, 1});

  auto query_word = "кот"s;
  auto doc1_v = SplitIntoWordsNoStop(doc1, stop_words_v);
  auto doc2_v = SplitIntoWordsNoStop(doc2, stop_words_v);

  auto found_docs = ss.FindTopDocuments(query_word);

  auto num_of_all_documents = ss.GetDocumentCount();
  auto num_of_found_documents = found_docs.size();
  double idf =
      log((double)num_of_all_documents / (double)num_of_found_documents);
  double tf1 = (double)count(doc1_v.begin(), doc1_v.end(), query_word)
      / (double)doc1_v.size();
  double tf2 = (double)count(doc2_v.begin(), doc2_v.end(), query_word)
      / (double)doc2_v.size();

  double tfidf1 = idf * tf1;
  double tfidf2 = idf * tf2;

  for (const auto &doc : found_docs) {
    if (doc.id == 0)
      ASSERT_HINT(abs(doc.relevance - tfidf1) < EPSILON,
                  "Incorrect document relevance"s);
    else if (doc.id == 1)
      ASSERT_HINT(abs(doc.relevance - tfidf2) < EPSILON,
                  "Incorrect document relevance"s);
  }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
  RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
  RUN_TEST(TestMinusWords);
  RUN_TEST(TestMatchingDocuments);
  RUN_TEST(TestSortRelevance);
  RUN_TEST(TestCalcRating);
  RUN_TEST(TestFilterPredicate);
  RUN_TEST(TestSearchDocumentsWithStatus);
  RUN_TEST(TestCalcRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

void PrintDocument(const Document& document) {
  cout << "{ "s
       << "document_id = "s << document.id << ", "s
       << "relevance = "s << document.relevance << ", "s
       << "rating = "s << document.rating
       << " }"s << endl;
}

int main() {
  TestSearchServer();
  // Если вы видите эту строку, значит все тесты прошли успешно
  cerr << "Search server testing finished"s << endl;

  SearchServer search_server("и в на"s);

  search_server.AddDocument(0, "белый кот и модный ошейник"s,
                            DocumentStatus::ACTUAL, {8, -3});
  search_server.AddDocument(1, "пушистый кот пушистый хвост"s,
                            DocumentStatus::ACTUAL, {7, 2, 7});
  search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s,
                            DocumentStatus::ACTUAL, {5, -12, 2, 1});

  for (const Document &document : search_server.FindTopDocuments(
      "ухоженный кот"s)) {
    PrintDocument(document);
  }

  return  0;
}
