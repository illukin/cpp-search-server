#include "document.h"

std::ostream &operator<<(std::ostream &os, const Document document) {
  using std::string_literals::operator""s;

  os << "{ "s
     << "document_id = "s << document.id << ", "s
     << "relevance = "s << document.relevance << ", "s
     << "rating = " << document.rating
     << " }"s;

  return os;
}
