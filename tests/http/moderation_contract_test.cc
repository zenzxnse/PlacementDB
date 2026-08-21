#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

int main() {
    const auto root = std::filesystem::path(PLACEDB_SOURCE_DIR);
    const auto route = Read(root / "api/src/http/moderation_routes.cc");
    const auto social = Read(root / "api/src/http/social_routes.cc");
    const auto questions = Read(root / "api/src/http/question_routes.cc");
    const auto migration = Read(
        root / "db/migrations/015_report_moderation_audit.sql");
    assert(route.find("Authorize(request, config, callback, true)") !=
           std::string::npos);
    assert(route.find("x.author_id<>$2::bigint") != std::string::npos);
    assert(route.find("set_config('placedb.reason'") != std::string::npos);
    assert(route.find("expected_state") != std::string::npos);
    assert(social.find("content_reports_one_open_idx") != std::string::npos);
    assert(social.find("/api/v1/reports") != std::string::npos);
    assert(social.find("/api/v1/me/submissions") != std::string::npos);
    assert(social.find("/api/v1/me/votes") != std::string::npos);
    assert(social.find("/api/v1/me/reports") != std::string::npos);
    assert(social.find("AND author_id<>$2 FOR SHARE") != std::string::npos);
    assert(questions.find("->newTransaction()") != std::string::npos);
    assert(questions.find("state='published' \"\n                            \"FOR UPDATE") !=
           std::string::npos);
    assert(social.find("limit + 1") != std::string::npos);
    assert(migration.find("append-only") != std::string::npos);
    assert(migration.find("REVOKE UPDATE, DELETE") != std::string::npos);
}
