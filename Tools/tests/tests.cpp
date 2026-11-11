#include "rag/harness.hpp"
#include "rag/analytics.hpp"
#include "rag/json.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

void write_text(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

std::string read_text(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

int main() {
    rag::PiiFilter pii("test");
    const auto filtered = pii.apply("Alex Sample wrote to alex@example.org from SW1A 1AA.");
    if (filtered.report.quarantined || filtered.text.find("Alex Sample") != std::string::npos ||
        filtered.text.find("alex@example.org") != std::string::npos ||
        filtered.text.find("SW1A 1AA") != std::string::npos) {
        std::cerr << "PII filter did not remove the test identifiers\n";
        return 1;
    }

    const auto fixture = fs::current_path() / "test-fixtures";
    write_text(fixture / "excerpts" / "article-22a.txt",
        "Article 22A: a significant decision has a legal or similarly significant effect.\n");
    write_text(fixture / "rules" / "rule.json",
        R"({"schema":"policy_rules_v1","tier":"statute","effective_from":"2026-09-30","clauses":[{"id":"Article 22A","text":"A significant automated decision needs meaningful human involvement.","tags":["automated decision","human review"],"source_url":"https://www.legislation.gov.uk/eur/2016/679/article/22A","excerpt_file":"../excerpts/article-22a.txt"}]})");
    write_text(fixture / "source" / "form.json",
        R"({"schema":"ico_personal_data_complaint_v1","case_reference":"TEST-1","complaint_type":"processing","issue_types":["automated decisions"],"date_discovered":"2026-10-10","respondent":{"name":"Example Council"},"complainant":{"name":"Alex Sample","email":"alex@example.org"},"narrative":"The automated decision rejected my application without a human review.","harm":"","desired_outcome":"Please review it.","supporting_evidence":[],"sharing_restrictions":"","declaration_agreed":true})");

    rag::Config config;
    config.source_dir = fixture / "source";
    config.rules_dir = fixture / "rules";
    config.output_dir = fixture / "output";
    const auto offline = rag::run(config);
    const auto offline_findings = rag::Json::parse(read_text(offline.run_dir / "CASE-1" / "findings.json"));
    if (offline.documents_processed != 1 || offline.clauses != 1 || offline.findings != 1 ||
        !fs::exists(offline.run_dir / "index.md") ||
        !fs::exists(offline.run_dir / "CASE-1" / "report.md") ||
        offline_findings.array()[0].at("case_id").string_or() != "CASE-1" ||
        offline_findings.array()[0].at("verdict").string_or() != "insufficient_information" ||
        !offline_findings.array()[0].at("confidence").is_null() ||
        offline_findings.array()[0].at("confidence_display").string_or().find("pending Hermes") == std::string::npos ||
        !offline_findings.array()[0].at("clause").at("id").string_or().empty() ||
        offline_findings.array()[0].at("candidate_rules").array().empty()) {
        std::cerr << "offline retrieval/report contract failed\n";
        return 2;
    }

#ifdef _WIN32
    config.use_hermes = true;
    config.hermes_executable = fs::path(TEST_SOURCE_DIR) / "tests" / "fake_hermes.cmd";
    config.hermes_timeout_seconds = 15;
    const auto live = rag::run(config);
    const auto live_finding = rag::Json::parse(read_text(live.run_dir / "CASE-1" / "findings.json")).array()[0];
    const auto live_prompt = read_text(live.run_dir / "CASE-1" / "hermes" / "claim-1" / "prompt.filtered.txt");
    if (live_finding.at("verdict").string_or() != "engaged" ||
        live_finding.at("clause").at("id").string_or() != "Article 22A" ||
        live_finding.at("confidence").number() < 0.8 ||
        live_finding.at("action_detail").string_or().find("Decision process records") == std::string::npos ||
        live_prompt.find("prepared_finding") == std::string::npos ||
        live_prompt.find("REVIEW_REQUIRED") == std::string::npos ||
        live_prompt.find("a significant decision has a legal or similarly significant effect") == std::string::npos) {
        std::cerr << "Hermes citation or string-confidence contract failed\n";
        return 3;
    }
    config.hermes_executable = fs::path(TEST_SOURCE_DIR) / "tests" / "fake_bad_hermes.cmd";
    const auto rejected = rag::run(config);
    const auto rejected_finding = rag::Json::parse(read_text(rejected.run_dir / "CASE-1" / "findings.json")).array()[0];
    if (rejected_finding.at("verdict").string_or() != "insufficient_information" ||
        !rejected_finding.at("clause").at("id").string_or().empty() ||
        rejected_finding.at("assessment_method").string_or() != "hermes_failed") {
        std::cerr << "invented AI citation was not rejected\n";
        return 4;
    }
    config.hermes_executable = fs::path(TEST_SOURCE_DIR) / "tests" / "fake_overstated_hermes.cmd";
    const auto overstated = rag::run(config);
    const auto overstated_finding = rag::Json::parse(read_text(overstated.run_dir / "CASE-1" / "findings.json")).array()[0];
    bool overstatement_flagged = false;
    for (const auto& flag : overstated_finding.at("flags").array())
        if (flag.string_or() == "MODEL_OVERSTATEMENT_REVIEW") overstatement_flagged = true;
    if (!overstatement_flagged || !overstated_finding.at("confidence").is_null() ||
        overstated_finding.at("confidence_reason").string_or().find("overstatement") == std::string::npos) {
        std::cerr << "overstated AI conclusion was not flagged\n";
        return 5;
    }
    config.hermes_executable = fs::path(TEST_SOURCE_DIR) / "tests" / "fake_article_quote_hermes.cmd";
    const auto article_quoted = rag::run(config);
    const auto article_finding = rag::Json::parse(read_text(article_quoted.run_dir / "CASE-1" / "findings.json")).array()[0];
    bool article_quote_flagged = false;
    for (const auto& flag : article_finding.at("flags").array())
        if (flag.string_or() == "ARTICLE_TEXT_QUOTE") article_quote_flagged = true;
    if (!article_quote_flagged || article_finding.at("verdict").string_or() != "engaged") {
        std::cerr << "exact Article text quote was not accepted\n";
        return 6;
    }
    config.hermes_executable = fs::path(TEST_SOURCE_DIR) / "tests" / "fake_punctuated_article_quote_hermes.cmd";
    const auto punctuation_quoted = rag::run(config);
    const auto punctuation_finding = rag::Json::parse(read_text(punctuation_quoted.run_dir / "CASE-1" / "findings.json")).array()[0];
    bool punctuation_flagged = false;
    for (const auto& flag : punctuation_finding.at("flags").array())
        if (flag.string_or() == "ARTICLE_TEXT_QUOTE_NORMALISED") punctuation_flagged = true;
    if (!punctuation_flagged || punctuation_finding.at("verdict").string_or() != "engaged") {
        std::cerr << "punctuation-only Article quote variation was not accepted\n";
        return 9;
    }
    config.hermes_executable = fs::path(TEST_SOURCE_DIR) / "tests" / "fake_named_action_hermes.cmd";
    const auto named_action = rag::run(config);
    const auto named_finding = rag::Json::parse(read_text(named_action.run_dir / "CASE-1" / "findings.json")).array()[0];
    bool response_redacted = false;
    for (const auto& flag : named_finding.at("flags").array())
        if (flag.string_or() == "MODEL_RESPONSE_REDACTED") response_redacted = true;
    if (!response_redacted || named_finding.at("assessment_method").string_or() != "hermes_llamacpp" ||
        named_finding.at("suggested_action").string_or() != "request_information" ||
        named_finding.at("action_detail").string_or().find("Example Council") != std::string::npos) {
        std::cerr << "model response redaction or action normalization failed\n";
        return 8;
    }
    config.hermes_executable = fs::path(TEST_SOURCE_DIR) / "tests" / "fake_typo_hermes.cmd";
    const auto typo_case = rag::run(config);
    const auto typo_finding = rag::Json::parse(read_text(typo_case.run_dir / "CASE-1" / "findings.json")).array()[0];
    if (typo_finding.at("assessment_method").string_or() != "hermes_llamacpp" ||
        typo_finding.at("confidence").number() != 0.85 ||
        typo_finding.at("suggested_action").string_or() != "request_information") {
        std::cerr << "recoverable Hermes field typo was not normalized\n";
        return 10;
    }
#endif
    config.use_hermes = false;
    write_text(fixture / "source" / "form2.json",
        R"({"schema":"ico_personal_data_complaint_v1","case_reference":"TEST-2","complaint_type":"processing","issue_types":["automated decisions"],"date_discovered":"2026-10-10","respondent":{"name":"Example Council"},"complainant":{"name":"Jordan Sample"},"narrative":"The automated decision rejected my application without a human review.","harm":"","desired_outcome":"Please review it.","supporting_evidence":[],"sharing_restrictions":"","declaration_agreed":true})");
    const auto grouped = rag::run(config);
    const auto grouped_index = read_text(grouped.run_dir / "index.md");
    if (grouped.documents_processed != 2 ||
        !fs::exists(grouped.run_dir / "CASE-1" / "findings.json") ||
        !fs::exists(grouped.run_dir / "CASE-2" / "findings.json") ||
        grouped_index.find("CASE-1") == std::string::npos ||
        grouped_index.find("CASE-2") == std::string::npos) {
        std::cerr << "per-case output layout failed\n";
        return 7;
    }
    const auto analytics = rag::export_analytics(grouped.run_dir, fixture / "analytics");
    const auto csv = read_text(analytics.csv_file);
    if (analytics.cases != 2 || analytics.findings != 2 ||
        csv.find("CASE-1") == std::string::npos ||
        csv.find("CASE-2") == std::string::npos ||
        csv.find("claim_id") == std::string::npos ||
        csv.find("Alex Sample") != std::string::npos ||
        csv.find("automated decision rejected") != std::string::npos ||
        csv.find("Please review it") != std::string::npos) {
        std::cerr << "analytics export contract or text minimisation failed\n";
        return 11;
    }
    fs::remove(fixture / "source" / "form2.json");

    write_text(fixture / "pack" / "pack.json",
        R"({"schema":"department_pack_v1","id":"hr-test","name":"HR Test","version":"1.0.0","rules":"rules","prompt":"prompt.txt","input":{"schema":"department_case_v1","reference_field":"reference","date_field":"event_date","organisation_field":"team","organisation_public":true,"outcome_field":"outcome","claim_fields":["summary"],"category_fields":["categories"],"flags":[{"field":"urgent","flag":"URGENT_REVIEW","mode":"true","severity":"high","action":"escalate_for_review"}]},"labels":{"organisation":"Team","outcome":"Outcome","rule":"Policy"}})");
    write_text(fixture / "pack" / "prompt.txt",
        "Assess this HR case against the supplied policies for a human reviewer.\n");
    write_text(fixture / "pack" / "rules" / "rules.json",
        R"({"schema":"policy_rules_v1","tier":"policy","clauses":[{"id":"HR-1","text":"A manager must document a significant automated employment decision and its human review.","tags":["automated decision","human review"],"review_action":"request the manager decision record","required_evidence":"Decision record and human review notes"}]})");
    write_text(fixture / "pack-source" / "case.json",
        R"({"schema":"department_case_v1","reference":"HR-101","event_date":"2026-10-10","team":"People Operations","categories":["automated decision"],"summary":"An automated decision rejected the request without a documented human review.","outcome":"Review the decision.","urgent":true})");
    rag::validate_pack(fixture / "pack");
    rag::Config pack_config;
    pack_config.pack_dir = fixture / "pack";
    pack_config.source_dir = fixture / "pack-source";
    pack_config.output_dir = fixture / "pack-output";
    const auto packed = rag::run(pack_config);
    const auto packed_finding = rag::Json::parse(read_text(packed.run_dir / "CASE-101" / "findings.json")).array()[0];
    const auto packed_report = read_text(packed.run_dir / "CASE-101" / "report.md");
    if (packed.documents_processed != 1 || packed.clauses != 1 ||
        packed_finding.at("suggested_action").string_or() != "escalate_for_review" ||
        packed_finding.at("missing_evidence").string_or() != "Decision record and human review notes" ||
        packed_report.find("Team: People Operations") == std::string::npos ||
        read_text(packed.run_dir / "manifest.json").find("\"department_pack\": \"hr-test\"") == std::string::npos) {
        std::cerr << "department pack mapping, labels or rule-owned actions failed\n";
        return 12;
    }
    std::cout << "all tests passed\n";
    return 0;
}
