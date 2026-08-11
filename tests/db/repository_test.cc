#include "db/repository.h"

#include <cassert>
#include <type_traits>

namespace {

void RepositoryInterfacesRemainConstructible() {
    using Client = const std::shared_ptr<drogon::orm::DbClient>&;
    static_assert(
        std::is_constructible_v<placedb::db::UserRepository, Client>);
    static_assert(
        std::is_constructible_v<placedb::db::QuestionRepository, Client>);
    static_assert(
        std::is_constructible_v<placedb::db::ExperienceRepository, Client>);
    static_assert(std::is_constructible_v<
                  placedb::db::DifficultyVoteRepository, Client>);
    static_assert(std::is_constructible_v<
                  placedb::db::ContentReportRepository, Client>);
    static_assert(
        std::is_constructible_v<placedb::db::SessionRepository, Client>);

    const auto result =
        placedb::db::Result<int>::Err(placedb::db::DbError::kUnavailable);
    assert(result.IsErr());
    assert(result.error() == placedb::db::DbError::kUnavailable);
}

}  // namespace

int main() { RepositoryInterfacesRemainConstructible(); }
