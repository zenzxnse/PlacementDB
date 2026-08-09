#ifndef PLACEDB_DTO_SERIALIZATION_H
#define PLACEDB_DTO_SERIALIZATION_H
#include "domain/types.h"
#include <string>
#include <string_view>
namespace placedb::dto {
std::string JsonString(std::string_view raw);
std::string ToJson(const domain::QuestionSummary& value);
std::string ToJson(const domain::Question& value);
std::string ToJson(const domain::ExperienceSummary& value);
std::string ToJson(const domain::Experience& value);
} /* namespace placedb::dto */
#endif
