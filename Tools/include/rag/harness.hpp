#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace rag {

namespace fs = std::filesystem;

enum class Disposition { Keep, Tokenise, Drop, Conditional };

struct Config {
    fs::path source_dir;
    fs::path rules_dir;
    fs::path output_dir;
    fs::path pack_dir;
    double retrieval_threshold = 0.12;
    double quarantine_threshold = 0.70;
    bool write_token_vault = false;
    std::string effective_date;
    bool use_hermes = false;
    fs::path hermes_executable;
    fs::path hermes_response_file;
    unsigned hermes_timeout_seconds = 300;
    unsigned max_documents = 0;
    std::string case_file;
};

struct PiiReport {
    std::map<std::string, int> removed_counts;
    std::map<std::string, int> tokenised_counts;
    double confidence = 1.0;
    bool quarantined = false;
    std::string reason;
};

struct FilteredText {
    std::string text;
    PiiReport report;
    std::map<std::string, std::string> vault;
};

struct Clause {
    std::string id;
    std::string path;
    std::string tier;
    std::string source_url;
    std::vector<std::string> tags;
    std::string effective_from;
    std::string effective_to;
    std::string text;
    std::string source_excerpt;
    std::string review_action;
    std::string required_evidence;
    fs::path source;
    int line = 0;
};

struct Claim {
    std::string id;
    std::string text;
    fs::path source;
    int line = 0;
    std::string location;
};

struct IcoComplaint {
    std::string case_reference;
    std::string complaint_type;
    std::vector<std::string> issue_types;
    std::string date_discovered;
    std::string date_complained_to_organisation;
    std::string last_contact_date;
    std::string respondent_name;
    std::string complainant_token;
    bool acting_on_behalf = false;
    bool under_16 = false;
    std::string narrative;
    std::string harm;
    std::string desired_outcome;
    std::vector<std::string> supporting_evidence;
    std::string sharing_restrictions;
    bool declaration_agreed = false;
};

struct Finding {
    std::string case_id;
    Claim claim;
    Clause clause;
    std::string verdict;
    std::string severity;
    double confidence = 0.0;
    std::string confidence_reason;
    std::string missing_evidence;
    std::string suggested_action;
    std::string action_detail;
    std::vector<std::string> flags;
    std::string assessment_method;
    double retrieval_score = 0.0;
    std::vector<std::string> candidate_paths;
};

struct RunSummary {
    int documents_seen = 0;
    int documents_processed = 0;
    int documents_quarantined = 0;
    int clauses = 0;
    int claims = 0;
    int findings = 0;
    fs::path run_dir;
};

class PiiFilter {
public:
    explicit PiiFilter(std::string token_salt = "local-run");
    FilteredText apply(const std::string& input, bool detect_untitled_names = true) const;
    std::string protect_field(const std::string& value, const std::string& category,
                              bool tokenise, PiiReport& report,
                              std::map<std::string, std::string>& vault) const;

private:
    std::string token_salt_;
    mutable std::map<std::string, std::string> token_lookup_;
    mutable unsigned next_token_ = 1;
};

RunSummary run(const Config& config);
void validate_pack(const fs::path& pack_dir);
std::string json_escape(const std::string& value);
IcoComplaint parse_ico_complaint(const std::string& raw_json, const PiiFilter& filter,
                                 PiiReport& report, std::map<std::string, std::string>& vault);

} // namespace rag
