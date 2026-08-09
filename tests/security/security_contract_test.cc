#include "auth/authorization.h"
#include "auth/csrf.h"
#include "auth/rate_limiter.h"
#include "auth/secret.h"
#include "dto/serialization.h"
#include "http/request_policy.h"
#include <cassert>
#include <chrono>
#include <string>
int main() {
  using namespace placedb;
  assert(auth::InitializeCryptoOnce());
  const auto token=auth::MintToken(); assert(token.size() >= 42 && token.find('=') == std::string::npos);
  assert(auth::HashToken(token).size()==64); assert(auth::SecretEquals(token,token)); assert(!auth::SecretEquals(token,token+"x"));
  const auto hash=auth::HashPassword("correct horse battery staple"); assert(hash.status==auth::PasswordHashStatus::kOk); assert(auth::VerifyPassword(hash.encoded,"correct horse battery staple")!=auth::PasswordVerifyResult::kRejected); assert(auth::VerifyPassword(hash.encoded,"wrong")==auth::PasswordVerifyResult::kRejected);
  const auto now=std::chrono::system_clock::now(); const auto csrf=auth::IssueLoginCsrf("test-only-mac-key",now); assert(auth::VerifyLoginCsrf(csrf,csrf,"test-only-mac-key",now)); assert(!auth::VerifyLoginCsrf(csrf,csrf,"wrong-key",now));
  assert(auth::RequireModerator(std::nullopt)==auth::AccessDecision::kAuthenticationRequired);
  auth::Principal moderator{1,domain::UserRole::kModerator,true,false}; assert(auth::RequireModerator(moderator)==auth::AccessDecision::kAllow); assert(auth::RequireDifferentActor(moderator,1)==auth::AccessDecision::kForbidden);
  auth::RateLimiter limiter({2,std::chrono::milliseconds(1000)}); const auto steady=std::chrono::steady_clock::now(); assert(limiter.Consume("client",steady).allowed); assert(limiter.Consume("client",steady).allowed); assert(!limiter.Consume("client",steady).allowed);
  assert(http::CheckJsonMutation("POST","application/json; charset=utf-8",10,20)==http::RequestPolicyDecision::kAllow); assert(http::CheckJsonMutation("POST","text/plain",10,20)==http::RequestPolicyDecision::kUnsupportedMediaType);
  domain::ExperienceSummary experience; experience.public_id_="id"; experience.slug_="slug"; experience.title_="<script>alert(1)</script>"; experience.outcome_visible_=false; experience.outcome_=domain::Outcome::kOffered; experience.published_at_="2026-08-10T00:00:00Z"; const auto json=dto::ToJson(experience); assert(json.find("<script>")!=std::string::npos); assert(json.find("\"outcome\":")==std::string::npos);
}
