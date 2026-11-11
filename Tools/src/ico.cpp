#include "rag/harness.hpp"
#include "rag/json.hpp"

#include <chrono>
#include <regex>
#include <set>
#include <stdexcept>

namespace rag {
namespace {

void allow_only(const Json& object, const std::set<std::string>& names, const std::string& path) {
    if (!object.is_object()) throw std::runtime_error(path + " must be an object");
    for (const auto& [name, value] : object.object()) {
        (void)value;
        if (!names.contains(name)) throw std::runtime_error("unexpected ICO field: " + path + "." + name);
    }
}

std::string string_field(const Json& root, const std::string& name, bool required = false) {
    const auto& value = root.at(name);
    if (value.is_string()) return value.string();
    if (required || !value.is_null()) throw std::runtime_error("ICO field must be text: " + name);
    return {};
}

bool boolean_field(const Json& root, const std::string& name) {
    const auto& value = root.at(name);
    return value.is_bool() && value.boolean();
}

std::string filtered(const PiiFilter& filter, const std::string& value, PiiReport& report,
                     std::map<std::string, std::string>& vault, bool detect_untitled_names = true) {
    const auto result = filter.apply(value, detect_untitled_names);
    if (result.report.quarantined) throw std::runtime_error(result.report.reason);
    for (const auto& [kind, count] : result.report.removed_counts) report.removed_counts[kind] += count;
    for (const auto& [kind, count] : result.report.tokenised_counts) report.tokenised_counts[kind] += count;
    vault.insert(result.vault.begin(), result.vault.end());
    return result.text;
}

void validate_date(const std::string& date, const std::string& name) {
    if (date.empty()) return;
    if (!std::regex_match(date, std::regex(R"(\d{4}-\d{2}-\d{2})")) ||
        !std::chrono::year_month_day{
            std::chrono::year(std::stoi(date.substr(0, 4))),
            std::chrono::month(static_cast<unsigned>(std::stoi(date.substr(5, 2)))),
            std::chrono::day(static_cast<unsigned>(std::stoi(date.substr(8, 2))))}.ok())
        throw std::runtime_error("ICO field must be a valid YYYY-MM-DD date: " + name);
}

} // namespace

IcoComplaint parse_ico_complaint(const std::string& raw_json, const PiiFilter& filter,
                                 PiiReport& report, std::map<std::string, std::string>& vault) {
    const auto root = Json::parse(raw_json);
    allow_only(root, {"schema", "case_reference", "complaint_type", "issue_types", "date_discovered",
                      "date_complained_to_organisation", "last_contact_date", "respondent",
                      "complainant", "narrative", "harm", "desired_outcome", "supporting_evidence",
                      "sharing_restrictions", "declaration_agreed"}, "root");
    if (string_field(root, "schema", true) != "ico_personal_data_complaint_v1")
        throw std::runtime_error("unsupported ICO complaint schema");
    IcoComplaint record;
    record.case_reference = filtered(filter, string_field(root, "case_reference", true), report, vault);
    record.complaint_type = string_field(root, "complaint_type", true);
    if (record.complaint_type != "access" && record.complaint_type != "processing")
        throw std::runtime_error("complaint_type must be access or processing");
    const auto& issue_types = root.at("issue_types");
    if (!issue_types.is_array()) throw std::runtime_error("issue_types must be an array");
    for (const auto& issue : issue_types.array()) {
        if (!issue.is_string()) throw std::runtime_error("issue_types must contain text");
        record.issue_types.push_back(filtered(filter, issue.string(), report, vault));
    }
    record.date_discovered = string_field(root, "date_discovered", true);
    record.date_complained_to_organisation = string_field(root, "date_complained_to_organisation");
    record.last_contact_date = string_field(root, "last_contact_date");
    validate_date(record.date_discovered, "date_discovered");
    if (record.date_discovered.empty()) throw std::runtime_error("date_discovered is required");
    validate_date(record.date_complained_to_organisation, "date_complained_to_organisation");
    validate_date(record.last_contact_date, "last_contact_date");

    const auto& respondent = root.at("respondent");
    allow_only(respondent, {"name", "contact_name", "email", "phone", "address"}, "respondent");
    record.respondent_name = filtered(filter, string_field(respondent, "name", true), report, vault, false);
    for (const auto& key : {"contact_name", "email", "phone", "address"})
        filter.protect_field(string_field(respondent, key), "RESPONDENT_CONTACT", false, report, vault);

    const auto& complainant = root.at("complainant");
    allow_only(complainant, {"name", "email", "phone", "address", "acting_on_behalf",
                             "person_represented", "under_16"}, "complainant");
    const auto name = string_field(complainant, "name", true);
    const auto email = string_field(complainant, "email");
    record.complainant_token = filter.protect_field(!name.empty() ? name : email,
                                                     "COMPLAINANT", true, report, vault);
    filter.protect_field(email, "CONTACT", true, report, vault);
    filter.protect_field(string_field(complainant, "phone"), "CONTACT", true, report, vault);
    filter.protect_field(string_field(complainant, "address"), "ADDRESS", false, report, vault);
    record.acting_on_behalf = boolean_field(complainant, "acting_on_behalf");
    record.under_16 = boolean_field(complainant, "under_16");
    filter.protect_field(string_field(complainant, "person_represented"), "REPRESENTED_PERSON", true, report, vault);

    record.narrative = filtered(filter, string_field(root, "narrative", true), report, vault);
    record.harm = filtered(filter, string_field(root, "harm"), report, vault);
    record.desired_outcome = filtered(filter, string_field(root, "desired_outcome"), report, vault);
    record.sharing_restrictions = filtered(filter, string_field(root, "sharing_restrictions"), report, vault);
    const auto& evidence = root.at("supporting_evidence");
    if (!evidence.is_array()) throw std::runtime_error("supporting_evidence must be an array");
    for (const auto& item : evidence.array()) {
        if (!item.is_string()) throw std::runtime_error("supporting_evidence must contain text");
        record.supporting_evidence.push_back(filtered(filter, item.string(), report, vault));
    }
    record.declaration_agreed = boolean_field(root, "declaration_agreed");
    if (record.case_reference.empty() || record.respondent_name.empty() || record.narrative.empty())
        throw std::runtime_error("case_reference, respondent.name and narrative are required");
    return record;
}

} // namespace rag
