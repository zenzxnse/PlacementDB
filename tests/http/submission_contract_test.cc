#include "http/submission_contract.h"
#include <cassert>
namespace placedb::http {
void SubmissionContractCases() {
    Json::Value q(Json::objectValue);
    q["title"]="Detect a cycle";
    q["prompt"]="Given a linked list, detect whether it contains a cycle.";
    q["state"]="published";
    assert(!ParseQuestionSubmission(q,2027).value);
    q.removeMember("state");
    q["topic_slugs"]=Json::arrayValue;
    q["topic_slugs"].append("linked-lists");
    const auto valid=ParseQuestionSubmission(q,2027);
    assert(valid.value&&valid.value->topic_slugs.size()==1);
    q["round"]="managerial";
    q["source_year"]=2028;
    assert(!ParseQuestionSubmission(q,2027).value);
    q["source_year"]=2027;
    assert(ParseQuestionSubmission(q,2027).value);
    for(int i=0;i<12;++i)q["topic_slugs"].append("topic-"+std::to_string(i));
    assert(!ParseQuestionSubmission(q,2027).value);

    Json::Value e(Json::objectValue);
    e["title"]="Acme interview";
    e["narrative"]="The process began with an assessment and two interviews.";
    e["outcome_visible"]=false;
    e["anonymous"]=true;
    e["rounds"]=Json::arrayValue;
    Json::Value round(Json::objectValue);
    round["round"]="online_assessment";
    e["rounds"].append(round);
    assert(ParseExperienceSubmission(e,2027).value);
    e["rounds"][0]["ordinal"]=1;
    assert(!ParseExperienceSubmission(e,2027).value);
    e["rounds"][0].removeMember("ordinal");
    e["outcome"]="offered";
    assert(!ParseExperienceSubmission(e,2027).value);
    assert(DeriveSubmissionSlug(" Hello, World! ","abcdef123456789")==
           "hello-world-abcdef123456");
}
}
