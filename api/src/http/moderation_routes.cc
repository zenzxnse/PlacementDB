#include "http/moderation_routes.h"

#include "app/request_executor.h"
#include "dto/serialization.h"
#include "http/api_error.h"
#include "http/current_user.h"
#include "http/request_context.h"

#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>

#include <atomic>
#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <set>

namespace placedb::http {
namespace {
using Callback = std::function<void(const drogon::HttpResponsePtr&)>;

drogon::HttpResponsePtr Json(std::string body,
                             drogon::HttpStatusCode status = drogon::k200OK) {
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(status);
    response->setContentTypeString("application/json; charset=utf-8");
    response->addHeader("Cache-Control", "no-store");
    response->setBody(std::move(body));
    return response;
}

drogon::HttpResponsePtr Error(ApiErrorCode code, std::string message,
                              const std::string& request_id) {
    auto error = ApiError::Make(code, std::move(message));
    error.SetRequestId(request_id);
    return Json(error.ToJson(),
                static_cast<drogon::HttpStatusCode>(error.HttpStatus()));
}

bool IsModerator(const AuthenticatedUser& user) {
    return user.user.role_name_ == "moderator" ||
           user.user.role_name_ == "administrator";
}

bool IsUuid(const std::string& value) {
    if (value.size() != 36) return false;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index == 8 || index == 13 || index == 18 || index == 23) {
            if (value[index] != '-') return false;
        } else if (!std::isxdigit(static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }
    return true;
}

bool KnownFields(const Json::Value& value,
                 const std::set<std::string>& allowed) {
    if (!value.isObject()) return false;
    for (const auto& key : value.getMemberNames()) {
        if (!allowed.contains(key)) return false;
    }
    return true;
}

std::optional<std::int32_t> PageLimit(const drogon::HttpRequestPtr& request) {
    const auto text = request->getParameter("limit");
    if (text.empty()) return 50;
    try {
        const auto value = std::stoi(text);
        if (value >= 1 && value <= 100) return value;
    } catch (...) {
    }
    return std::nullopt;
}

template <typename Work>
void Dispatch(const std::shared_ptr<app::RequestExecutor>& executor,
              Callback&& callback, Work&& work) {
    auto completed = std::make_shared<std::atomic_bool>(false);
    auto output = std::make_shared<Callback>(std::move(callback));
    auto unavailable = [completed, output] {
        if (!completed->exchange(true)) {
            (*output)(Error(ApiErrorCode::kServiceUnavailable,
                            "The server is busy. Try again shortly.", ""));
        }
    };
    if (!executor || !executor->Submit(
        [completed, output, unavailable,
         task = std::forward<Work>(work)]() mutable {
            try {
                task(Callback([completed, output](const auto& response) {
                    if (!completed->exchange(true)) (*output)(response);
                }));
            } catch (...) {
                unavailable();
            }
        })) unavailable();
}

std::optional<AuthenticatedUser> Authorize(
    const drogon::HttpRequestPtr& request, const config::ServerConfig& config,
    Callback& callback, bool mutation) {
    const auto current = AuthenticateRequest(request, config);
    const auto request_id = SelectRequestId(
        request->getHeader("x-request-id"), true);
    if (!current) {
        callback(Error(ApiErrorCode::kAuthRequired,
                       "Sign in is required.", request_id));
        return std::nullopt;
    }
    if (!IsModerator(*current)) {
        callback(Error(ApiErrorCode::kForbidden,
                       "Moderator access is required.", request_id));
        return std::nullopt;
    }
    if (mutation &&
        !TrustedAuthenticatedMutation(request, config, *current)) {
        callback(Error(ApiErrorCode::kCsrfFailed,
                       "The CSRF token was not accepted.", request_id));
        return std::nullopt;
    }
    return current;
}

void Queue(const drogon::HttpRequestPtr& request,
           const config::ServerConfig& config, Callback callback) {
    if (!Authorize(request, config, callback, false)) return;
    const auto limit = PageLimit(request);
    const auto cursor = request->getParameter("cursor");
    std::string cursor_type;
    std::string cursor_id;
    if (!cursor.empty()) {
        const auto split = cursor.find(':');
        if (split == std::string::npos) {
            callback(Error(ApiErrorCode::kValidationFailed,
                           "The queue cursor is invalid.", "")); return;
        }
        cursor_type = cursor.substr(0, split);
        cursor_id = cursor.substr(split + 1);
    }
    if (!limit || (!cursor.empty() &&
        ((cursor_type != "question" && cursor_type != "experience") ||
         !IsUuid(cursor_id)))) {
        callback(Error(ApiErrorCode::kValidationFailed,
                       "The queue cursor or limit is invalid.", "")); return;
    }
    try {
        const auto rows = drogon::app().getDbClient("default")->execSqlSync(
            "WITH items AS (SELECT 'question' target_type,q.id,q.public_id,q.title,"
            "u.username::text,u.display_name,q.created_at FROM questions q "
            "JOIN users u ON u.id=q.author_id WHERE q.state='pending_review' "
            "UNION ALL SELECT 'experience',e.id,e.public_id,e.title,"
            "u.username::text,u.display_name,e.created_at FROM experiences e "
            "JOIN users u ON u.id=e.author_id WHERE e.state='pending_review'),"
            "anchor AS (SELECT created_at,target_type,id FROM items WHERE "
            "target_type=NULLIF($1,'') AND public_id=NULLIF($2,'')::uuid) "
            "SELECT target_type,public_id::text,title,username,display_name,created_at "
            "FROM items WHERE ($1='' OR (created_at,target_type,id)>(SELECT "
            "created_at,target_type,id FROM anchor)) ORDER BY created_at,target_type,id LIMIT $3",
            cursor_type, cursor_id, *limit + 1);
        if (!cursor.empty() && rows.empty()) {
            callback(Error(ApiErrorCode::kValidationFailed,
                           "The queue cursor is invalid.", "")); return;
        }
        const auto count = std::min<std::size_t>(
            rows.size(), static_cast<std::size_t>(*limit));
        std::string body = "{\"items\":[";
        for (std::size_t index = 0; index < count; ++index) {
            if (index) body.push_back(',');
            const auto row = rows[index];
            body += "{\"target_type\":" +
                dto::JsonString(row["target_type"].as<std::string>());
            body += ",\"public_id\":" +
                dto::JsonString(row["public_id"].as<std::string>());
            body += ",\"title\":" +
                dto::JsonString(row["title"].as<std::string>());
            body += ",\"state\":\"pending_review\",\"author\":{\"username\":" +
                dto::JsonString(row["username"].as<std::string>());
            body += ",\"display_name\":" +
                dto::JsonString(row["display_name"].as<std::string>()) + "}}";
        }
        body += "],\"next_cursor\":";
        body += rows.size() > count
            ? dto::JsonString(rows[count - 1]["target_type"].as<std::string>() +
                              ":" + rows[count - 1]["public_id"].as<std::string>())
            : "null";
        body += "}";
        callback(Json(std::move(body)));
    } catch (const drogon::orm::DrogonDbException&) {
        callback(Error(ApiErrorCode::kServiceUnavailable,
                       "The moderation queue is unavailable.", ""));
    }
}

void Action(const drogon::HttpRequestPtr& request,
            const config::ServerConfig& config, const std::string& public_id,
            Callback callback) {
    const auto current = Authorize(request, config, callback, true);
    if (!current) return;
    const auto json = request->getJsonObject();
    if (!json || !KnownFields(*json,
        {"target_type", "action", "expected_state", "reason"}) ||
        !(*json)["target_type"].isString() || !(*json)["action"].isString() ||
        !(*json)["expected_state"].isString() || !(*json)["reason"].isString() ||
        !IsUuid(public_id)) {
        callback(Error(ApiErrorCode::kValidationFailed,
                       "The moderation action was not valid.", ""));
        return;
    }
    const std::string type = (*json)["target_type"].asString();
    const std::string action = (*json)["action"].asString();
    const std::string expected = (*json)["expected_state"].asString();
    const std::string reason = (*json)["reason"].asString();
    static const std::map<std::string, std::string> states{
        {"approve", "published"}, {"request_changes", "changes_requested"},
        {"reject", "rejected"}, {"hide", "hidden"},
        {"restore", "published"}};
    const auto next = states.find(action);
    if ((type != "question" && type != "experience") ||
        next == states.end() || reason.empty() || reason.size() > 1000) {
        callback(Error(ApiErrorCode::kValidationFailed,
                       "The moderation action was not valid.", ""));
        return;
    }
    try {
        auto transaction = drogon::app().getDbClient("default")->newTransaction();
        const std::string table = type == "question" ? "questions" : "experiences";
        const std::string sql =
            "WITH configured AS (SELECT set_config('placedb.actor_id',$2,true),"
            "set_config('placedb.reason',$5,true),set_config('placedb.request_id',$6,true)) "
            "UPDATE " + table + " x SET state=$4,updated_at=now() FROM configured "
            "WHERE x.public_id=$1::uuid AND x.state=$3 AND x.author_id<>$2::bigint "
            "RETURNING x.public_id::text";
        const auto rows = transaction->execSqlSync(
            sql, public_id, std::to_string(current->user.id_), expected,
            next->second, reason, SelectRequestId(
                request->getHeader("x-request-id"), true));
        if (rows.empty()) {
            transaction->rollback();
            callback(Error(ApiErrorCode::kConflict,
                           "The content state changed before this decision.", ""));
            return;
        }
        callback(Json("{\"target_type\":" + dto::JsonString(type) +
                      ",\"public_id\":" + dto::JsonString(public_id) +
                      ",\"state\":" + dto::JsonString(next->second) + "}"));
    } catch (const drogon::orm::DrogonDbException&) {
        callback(Error(ApiErrorCode::kConflict,
                       "The action conflicts with the current state.", ""));
    }
}

void HideComment(const drogon::HttpRequestPtr& request,
                 const config::ServerConfig& config,
                 const std::string& public_id, Callback callback) {
    const auto current = Authorize(request, config, callback, true);
    if (!current) return;
    const auto json = request->getJsonObject();
    if (!json || !KnownFields(*json, {"reason"}) ||
        !(*json)["reason"].isString() || (*json)["reason"].asString().empty() ||
        (*json)["reason"].asString().size() > 1000 || !IsUuid(public_id)) {
        callback(Error(ApiErrorCode::kValidationFailed,
                       "A moderation reason is required.", ""));
        return;
    }
    try {
        auto transaction = drogon::app().getDbClient("default")->newTransaction();
        const auto rows = transaction->execSqlSync(
            "WITH configured AS (SELECT set_config('placedb.actor_id',$2,true),"
            "set_config('placedb.reason',$3,true),set_config('placedb.request_id',$4,true)) "
            "UPDATE comments c SET state='hidden',updated_at=now() FROM configured "
            "WHERE c.public_id=$1::uuid AND c.state='visible' RETURNING c.public_id",
            public_id, std::to_string(current->user.id_),
            (*json)["reason"].asString(), SelectRequestId(
                request->getHeader("x-request-id"), true));
        if (rows.empty()) {
            transaction->rollback();
            callback(Error(ApiErrorCode::kConflict,
                           "The comment state changed before this decision.", ""));
            return;
        }
        callback(Json("{\"public_id\":" + dto::JsonString(public_id) +
                      ",\"state\":\"hidden\"}"));
    } catch (const drogon::orm::DrogonDbException&) {
        callback(Error(ApiErrorCode::kServiceUnavailable,
                       "The comment could not be hidden.", ""));
    }
}

void Reports(const drogon::HttpRequestPtr& request,
             const config::ServerConfig& config, Callback callback) {
    if (!Authorize(request, config, callback, false)) return;
    const std::string state = request->getParameter("state").empty()
        ? "open" : request->getParameter("state");
    static const std::set<std::string> states{
        "open", "under_review", "resolved", "dismissed"};
    const auto limit = PageLimit(request);
    const auto cursor = request->getParameter("cursor");
    if (!states.contains(state) || !limit ||
        (!cursor.empty() && !IsUuid(cursor))) {
        callback(Error(ApiErrorCode::kValidationFailed,
                       "The report state, cursor, or limit is invalid.", ""));
        return;
    }
    try {
        const auto rows = drogon::app().getDbClient("default")->execSqlSync(
            "SELECT cr.public_id::text,cr.target_type,cr.reason,cr.details,cr.state,"
            "u.username::text reporter,to_char(cr.created_at AT TIME ZONE 'UTC',"
            "'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') created_at FROM content_reports cr "
            "JOIN users u ON u.id=cr.reporter_id WHERE cr.state=$1 AND "
            "($2='' OR (cr.created_at,cr.id)>(SELECT created_at,id FROM content_reports "
            "WHERE public_id=$2::uuid AND state=$1)) ORDER BY cr.created_at,cr.id LIMIT $3",
            state, cursor, *limit + 1);
        if (!cursor.empty() && rows.empty()) {
            callback(Error(ApiErrorCode::kValidationFailed,
                           "The report cursor is invalid.", "")); return;
        }
        const auto count = std::min<std::size_t>(
            rows.size(), static_cast<std::size_t>(*limit));
        std::string body = "{\"items\":[";
        for (std::size_t index = 0; index < count; ++index) {
            if (index) body.push_back(',');
            const auto row = rows[index];
            body += "{\"public_id\":" + dto::JsonString(row["public_id"].as<std::string>());
            body += ",\"target_type\":" + dto::JsonString(row["target_type"].as<std::string>());
            body += ",\"reason\":" + dto::JsonString(row["reason"].as<std::string>());
            body += ",\"details\":" + (row["details"].isNull() ? std::string("null") : dto::JsonString(row["details"].as<std::string>()));
            body += ",\"state\":" + dto::JsonString(row["state"].as<std::string>());
            body += ",\"reporter_label\":" + dto::JsonString(row["reporter"].as<std::string>());
            body += ",\"created_at\":" + dto::JsonString(row["created_at"].as<std::string>()) + "}";
        }
        body += "],\"next_cursor\":";
        body += rows.size() > count
            ? dto::JsonString(rows[count - 1]["public_id"].as<std::string>())
            : "null";
        body += "}";
        callback(Json(std::move(body)));
    } catch (const drogon::orm::DrogonDbException&) {
        callback(Error(ApiErrorCode::kServiceUnavailable,
                       "Reports are temporarily unavailable.", ""));
    }
}

void ResolveReport(const drogon::HttpRequestPtr& request,
                   const config::ServerConfig& config,
                   const std::string& public_id, Callback callback) {
    const auto current = Authorize(request, config, callback, true);
    if (!current) return;
    const auto json = request->getJsonObject();
    if (!json || !KnownFields(*json,
        {"expected_state", "decision", "reason"}) ||
        !(*json)["expected_state"].isString() ||
        !(*json)["decision"].isString() || !(*json)["reason"].isString() ||
        !IsUuid(public_id)) {
        callback(Error(ApiErrorCode::kValidationFailed,
                       "The report decision was not valid.", ""));
        return;
    }
    const std::string expected = (*json)["expected_state"].asString();
    const std::string decision = (*json)["decision"].asString();
    const std::string reason = (*json)["reason"].asString();
    if ((decision != "resolved" && decision != "dismissed") ||
        reason.empty() || reason.size() > 1000) {
        callback(Error(ApiErrorCode::kValidationFailed,
                       "The report decision was not valid.", ""));
        return;
    }
    try {
        auto transaction = drogon::app().getDbClient("default")->newTransaction();
        const auto rows = transaction->execSqlSync(
            "WITH changed AS (UPDATE content_reports SET state=$3,resolved_by=$2,"
            "resolved_at=now(),resolution_note=$4,updated_at=now() WHERE public_id=$1::uuid "
            "AND state=$5 RETURNING id), audited AS (INSERT INTO report_moderation_events "
            "(report_id,actor_id,previous_state,new_state,reason,request_id) "
            "SELECT id,$2,$5,$3,$4,$6 FROM changed RETURNING report_id) SELECT report_id FROM audited",
            public_id, current->user.id_, decision, reason, expected,
            SelectRequestId(request->getHeader("x-request-id"), true));
        if (rows.empty()) {
            transaction->rollback();
            callback(Error(ApiErrorCode::kConflict,
                           "The report state changed before this decision.", ""));
            return;
        }
        callback(Json("{\"public_id\":" + dto::JsonString(public_id) +
                      ",\"state\":" + dto::JsonString(decision) + "}"));
    } catch (const drogon::orm::DrogonDbException&) {
        callback(Error(ApiErrorCode::kServiceUnavailable,
                       "The report could not be resolved.", ""));
    }
}

void Audit(const drogon::HttpRequestPtr& request,
           const config::ServerConfig& config, Callback callback) {
    if (!Authorize(request, config, callback, false)) return;
    const auto limit = PageLimit(request);
    const auto cursor = request->getParameter("cursor");
    std::string cursor_kind;
    std::int64_t cursor_id = 0;
    if (!cursor.empty()) {
        const auto split = cursor.find(':');
        try {
            if (split == std::string::npos) throw std::invalid_argument("cursor");
            cursor_kind = cursor.substr(0, split);
            cursor_id = std::stoll(cursor.substr(split + 1));
        } catch (...) {
            callback(Error(ApiErrorCode::kValidationFailed,
                           "The audit cursor is invalid.", "")); return;
        }
    }
    if (!limit || (!cursor.empty() &&
        (cursor_id < 1 || (cursor_kind != "content" &&
         cursor_kind != "comment" && cursor_kind != "report")))) {
        callback(Error(ApiErrorCode::kValidationFailed,
                       "The audit cursor or limit is invalid.", "")); return;
    }
    try {
        const auto rows = drogon::app().getDbClient("default")->execSqlSync(
            "WITH events AS (SELECT 'content' event_kind,m.target_type,CASE WHEN m.target_type='question' "
            "THEN q.public_id ELSE e.public_id END public_id,m.previous_state,m.new_state,m.reason,"
            "m.actor_role,m.created_at,m.id FROM moderation_events m LEFT JOIN questions q ON "
            "m.target_type='question' AND q.id=m.target_id LEFT JOIN experiences e ON "
            "m.target_type='experience' AND e.id=m.target_id UNION ALL SELECT 'comment','comment',"
            "c.public_id,m.previous_state,m.new_state,m.reason,r.name,m.created_at,m.id FROM "
            "comment_moderation_events m JOIN comments c ON c.id=m.comment_id JOIN users u ON "
            "u.id=m.actor_id JOIN roles r ON r.id=u.role_id UNION ALL SELECT 'report','report',"
            "cr.public_id,m.previous_state,m.new_state,m.reason,r.name,m.created_at,m.id FROM "
            "report_moderation_events m JOIN content_reports cr ON cr.id=m.report_id JOIN users u "
            "ON u.id=m.actor_id JOIN roles r ON r.id=u.role_id), anchor AS (SELECT created_at,"
            "event_kind,id FROM events WHERE event_kind=NULLIF($1,'') AND id=$2) "
            "SELECT event_kind,target_type,public_id,previous_state,new_state,reason,actor_role,"
            "created_at::text,id FROM events WHERE ($1='' OR (created_at,event_kind,id)<"
            "(SELECT created_at,event_kind,id FROM anchor)) ORDER BY created_at DESC,event_kind DESC,"
            "id DESC LIMIT $3", cursor_kind, cursor_id, *limit + 1);
        if (!cursor.empty() && rows.empty()) {
            callback(Error(ApiErrorCode::kValidationFailed,
                           "The audit cursor is invalid.", "")); return;
        }
        const auto count = std::min<std::size_t>(
            rows.size(), static_cast<std::size_t>(*limit));
        std::string body = "{\"items\":[";
        for (std::size_t index = 0; index < count; ++index) {
            if (index) body.push_back(',');
            const auto row = rows[index];
            body += "{\"event_kind\":" + dto::JsonString(row["event_kind"].as<std::string>());
            body += ",\"target_type\":" + dto::JsonString(row["target_type"].as<std::string>());
            body += ",\"public_id\":" + dto::JsonString(row["public_id"].as<std::string>());
            body += ",\"previous_state\":" + dto::JsonString(row["previous_state"].as<std::string>());
            body += ",\"new_state\":" + dto::JsonString(row["new_state"].as<std::string>());
            body += ",\"reason\":" + (row["reason"].isNull() ? std::string("null") : dto::JsonString(row["reason"].as<std::string>()));
            body += ",\"actor_role\":" + dto::JsonString(row["actor_role"].as<std::string>());
            body += ",\"created_at\":" + dto::JsonString(row["created_at"].as<std::string>()) + "}";
        }
        body += "],\"next_cursor\":";
        body += rows.size() > count
            ? dto::JsonString(rows[count - 1]["event_kind"].as<std::string>() +
                              ":" + std::to_string(rows[count - 1]["id"].as<std::int64_t>()))
            : "null";
        body += "}";
        callback(Json(std::move(body)));
    } catch (const drogon::orm::DrogonDbException&) {
        callback(Error(ApiErrorCode::kServiceUnavailable,
                       "The moderation audit is unavailable.", ""));
    }
}
}

void RegisterModerationRoutes(
    const config::ServerConfig& config,
    const std::shared_ptr<app::RequestExecutor>& request_db) {
    auto state = std::make_shared<config::ServerConfig>(config);
    drogon::app().registerHandler("/api/v1/moderation/queue",
        [state, request_db](const drogon::HttpRequestPtr& request, Callback&& callback) {
            Dispatch(request_db, std::move(callback), [state, request](Callback output) {
                Queue(request, *state, std::move(output));
            });
        }, {drogon::Get});
    drogon::app().registerHandler("/api/v1/moderation/items/{1}/action",
        [state, request_db](const drogon::HttpRequestPtr& request, Callback&& callback,
                            const std::string& public_id) {
            Dispatch(request_db, std::move(callback), [state, request, public_id](Callback output) {
                Action(request, *state, public_id, std::move(output));
            });
        }, {drogon::Post});
    drogon::app().registerHandler("/api/v1/moderation/comments/{1}/hide",
        [state, request_db](const drogon::HttpRequestPtr& request, Callback&& callback,
                            const std::string& public_id) {
            Dispatch(request_db, std::move(callback), [state, request, public_id](Callback output) {
                HideComment(request, *state, public_id, std::move(output));
            });
        }, {drogon::Post});
    drogon::app().registerHandler("/api/v1/moderation/audit",
        [state, request_db](const drogon::HttpRequestPtr& request, Callback&& callback) {
            Dispatch(request_db, std::move(callback), [state, request](Callback output) {
                Audit(request, *state, std::move(output));
            });
        }, {drogon::Get});
    drogon::app().registerHandler("/api/v1/moderation/reports",
        [state, request_db](const drogon::HttpRequestPtr& request, Callback&& callback) {
            Dispatch(request_db, std::move(callback), [state, request](Callback output) {
                Reports(request, *state, std::move(output));
            });
        }, {drogon::Get});
    drogon::app().registerHandler("/api/v1/moderation/reports/{1}/resolve",
        [state, request_db](const drogon::HttpRequestPtr& request, Callback&& callback,
                            const std::string& public_id) {
            Dispatch(request_db, std::move(callback), [state, request, public_id](Callback output) {
                ResolveReport(request, *state, public_id, std::move(output));
            });
        }, {drogon::Post});
}
}
