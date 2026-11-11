#include "rag/analytics.hpp"
#include "rag/json.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace rag {
namespace {
namespace fs = std::filesystem;

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read " + path.string());
    std::ostringstream result;
    result << input.rdbuf();
    return result.str();
}

std::string csv(const std::string& value) {
    std::string escaped = "\"";
    for (char ch : value) {
        if (ch == '"') escaped += "\"\"";
        else if (ch == '\r' || ch == '\n') escaped += ' ';
        else escaped += ch;
    }
    return escaped + '"';
}

std::string field(const Json& object, const char* name) {
    return object.at(name).string_or();
}

std::string number_field(const Json& object, const char* name) {
    const auto& value = object.at(name);
    if (value.is_null()) return "";
    if (!value.is_number()) throw std::runtime_error(std::string("invalid numeric finding field: ") + name);
    return value.dump();
}

} // namespace

AnalyticsSummary export_analytics(const fs::path& cases_dir, const fs::path& analytics_dir) {
    if (!fs::is_directory(cases_dir)) throw std::runtime_error("case output directory does not exist: " + cases_dir.string());
    std::vector<fs::path> case_dirs;
    for (const auto& entry : fs::directory_iterator(cases_dir)) {
        if (entry.is_directory() && entry.path().filename().string().starts_with("CASE-"))
            case_dirs.push_back(entry.path());
    }
    std::sort(case_dirs.begin(), case_dirs.end());
    if (case_dirs.empty()) throw std::runtime_error("no CASE-* folders found in " + cases_dir.string());

    std::ostringstream rows;
    rows << "case_id,claim_id,verdict,severity,assessment_method,clause_id,clause_path,source_url,"
            "suggested_action,flags,retrieval_score,model_confidence,confidence_available,review_required\n";
    AnalyticsSummary summary;
    std::set<std::string> keys;
    for (const auto& dir : case_dirs) {
        const auto manifest = Json::parse(read_file(dir / "manifest.json"));
        const auto findings = Json::parse(read_file(dir / "findings.json"));
        const auto case_id = dir.filename().string();
        if (!manifest.is_object() || field(manifest, "folder") != case_id ||
            !findings.is_array() || findings.array().empty())
            throw std::runtime_error("invalid case manifest or findings: " + case_id);
        ++summary.cases;
        for (const auto& finding : findings.array()) {
            if (!finding.is_object() || field(finding, "case_id") != case_id)
                throw std::runtime_error("finding case ID mismatch: " + case_id);
            const auto claim_id = field(finding, "claim_id");
            if (claim_id.empty() || !keys.insert(case_id + "/" + claim_id).second)
                throw std::runtime_error("missing or duplicate claim ID in " + case_id);
            std::string flags;
            bool review = false;
            const auto& flag_array = finding.at("flags");
            if (!flag_array.is_array()) throw std::runtime_error("invalid flags in " + case_id);
            for (const auto& item : flag_array.array()) {
                if (!item.is_string()) throw std::runtime_error("invalid flag in " + case_id);
                if (!flags.empty()) flags += '|';
                flags += item.string();
                review = review || item.string() == "REVIEW_REQUIRED";
            }
            const auto confidence = number_field(finding, "confidence");
            const auto& clause = finding.at("clause");
            const std::vector<std::string> cells = {
                case_id, claim_id, field(finding, "verdict"), field(finding, "severity"),
                field(finding, "assessment_method"), field(clause, "id"), field(clause, "path"),
                field(clause, "source_url"), field(finding, "suggested_action"), flags,
                number_field(finding, "retrieval_score"), confidence,
                confidence.empty() ? "false" : "true", review ? "true" : "false"
            };
            for (std::size_t i = 0; i < cells.size(); ++i) {
                if (i) rows << ',';
                rows << csv(cells[i]);
            }
            rows << '\n';
            ++summary.findings;
        }
    }
    fs::create_directories(analytics_dir);
    summary.csv_file = analytics_dir / "findings.csv";
    std::ofstream output(summary.csv_file, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write " + summary.csv_file.string());
    output << rows.str();
    if (!output) throw std::runtime_error("failed writing " + summary.csv_file.string());
    return summary;
}

} // namespace rag
