#include "search/meilisearch_index.h"

#include <json/json.h>

#include <cctype>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace placedb::search {
namespace {

constexpr const char* kHttpPrefix = "http://";
constexpr const char* kHttpsPrefix = "https://";

bool ValidIndexUid(const std::string& uid) {
    if (uid.empty()) {
        return false;
    }
    for (const char raw_byte : uid) {
        const auto byte = static_cast<unsigned char>(raw_byte);
        if (!std::isalnum(byte) && raw_byte != '-' && raw_byte != '_') {
            return false;
        }
    }
    return true;
}

bool ValidTarget(const std::string& target_type, std::int64_t target_id) {
    return target_id > 0
        && (target_type == "question" || target_type == "experience");
}

} /* namespace */

MeilisearchIndex::MeilisearchIndex(MeilisearchSettings settings)
    : settings_(std::move(settings)) {
    const bool http = settings_.base_url_.rfind(kHttpPrefix, 0) == 0;
    const bool https = settings_.base_url_.rfind(kHttpsPrefix, 0) == 0;
    const std::string host = http
        ? settings_.base_url_.substr(7)
        : settings_.base_url_.substr(8);
    if ((!http && !https) || host.empty() || host.find('/') != std::string::npos) {
        throw std::invalid_argument(
            "Meilisearch base URL must be http://host[:port] or "
            "https://host[:port] without a path");
    }
    if (!ValidIndexUid(settings_.index_uid_)) {
        throw std::invalid_argument(
            "Meilisearch index uid must be non-empty and use letters, "
            "digits, hyphens, and underscores only");
    }
    if (!std::isfinite(settings_.timeout_seconds_)
        || settings_.timeout_seconds_ <= 0.0) {
        throw std::invalid_argument(
            "Meilisearch timeout must be a positive finite number of seconds");
    }
    loop_thread_ = std::thread([this] { loop_.loop(); });
    http_ = drogon::HttpClient::newHttpClient(settings_.base_url_, &loop_);
}

MeilisearchIndex::~MeilisearchIndex() {
    loop_.quit();
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
    http_.reset();
}

std::string MeilisearchIndex::DocumentId(const std::string& target_type,
                                         std::int64_t target_id) {
    return target_type + "-" + std::to_string(target_id);
}

bool MeilisearchIndex::Upsert(const SearchDocument& document) {
    if (!ValidTarget(document.target_type_, document.target_id_)
        || document.public_id_.empty() || document.slug_.empty()) {
        return false;
    }
    /*
     * Every field travels on every upsert. Meilisearch's PUT is a partial
     * update by top-level field, so sending the full shape keeps a stale
     * company or year from surviving a content edit, and explicit nulls clear
     * what is now absent.
     */
    Json::Value doc;
    doc["id"] = DocumentId(document.target_type_, document.target_id_);
    doc["kind"] = document.target_type_;
    doc["public_id"] = document.public_id_;
    doc["slug"] = document.slug_;
    doc["title"] = document.title_;
    doc["body"] = document.body_;
    if (document.company_.has_value()) {
        doc["company"] = *document.company_;
    } else {
        doc["company"] = Json::nullValue;
    }
    if (document.job_role_.has_value()) {
        doc["job_role"] = *document.job_role_;
    } else {
        doc["job_role"] = Json::nullValue;
    }
    if (document.source_year_.has_value()) {
        doc["source_year"] = *document.source_year_;
    } else {
        doc["source_year"] = Json::nullValue;
    }
    Json::Value payload(Json::arrayValue);
    payload.append(std::move(doc));

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Put);
    request->setPath("/indexes/" + settings_.index_uid_ + "/documents");
    request->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    request->setBody(Json::writeString(builder, payload));
    return Send(std::move(request));
}

bool MeilisearchIndex::Remove(const std::string& target_type,
                              std::int64_t target_id) {
    if (!ValidTarget(target_type, target_id)) {
        return false;
    }
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Delete);
    request->setPath("/indexes/" + settings_.index_uid_ + "/documents/"
                     + DocumentId(target_type, target_id));
    return Send(std::move(request));
}

bool MeilisearchIndex::Send(drogon::HttpRequestPtr request) const {
    if (!http_) {
        return false;
    }
    if (!settings_.api_key_.empty()) {
        request->addHeader("Authorization", "Bearer " + settings_.api_key_);
    }
    /*
     * Synchronous send from the worker thread. Drogon forbids this call only
     * on the client's own loop thread, and the loop here runs on the thread
     * started by the constructor, never on the caller's.
     */
    const auto [result, response] =
        http_->sendRequest(request, settings_.timeout_seconds_);
    if (result != drogon::ReqResult::Ok || !response) {
        return false;
    }
    const int status = static_cast<int>(response->getStatusCode());
    return status >= 200 && status < 300;
}

} /* namespace placedb::search */
