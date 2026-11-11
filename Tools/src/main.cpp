#include "rag/harness.hpp"
#include "rag/analytics.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace {

void usage() {
    std::cout
        << "Department-pack case/rule RAG harness\n\n"
        << "Usage:\n"
        << "  rag-cli run [--pack Policy] [--source Tools/Input] [--output Output] [options]\n"
        << "  rag-cli validate-pack --pack FOLDER\n\n"
        << "  rag-cli export-analytics [--output Output] [--analytics-dir Output/Analytics]\n\n"
        << "Options:\n"
        << "  --pack FOLDER                 Load rules, prompt, field mapping and labels from a department pack\n"
        << "  --rules FOLDER                Override the pack rule folder (legacy/default: Policy/rules)\n"
        << "  --effective-date YYYY-MM-DD   Select rules in force on the event date\n"
        << "  --threshold N                 Retrieval score threshold (default 0.12)\n"
        << "  --write-token-vault           Write a sensitive token mapping (off by default)\n"
        << "  --hermes                      Assess claims with local Hermes (the default)\n"
        << "  --offline                     Retrieve rules without calling Hermes\n"
        << "  --hermes-cli PATH             Override the Hermes CLI path\n"
        << "  --replay-response FILE        Revalidate one saved Hermes JSON response with --case\n"
        << "  --hermes-timeout SECONDS      Per-claim timeout (default 300)\n"
        << "  --max-documents N             Process first N source files (sorted; default all)\n"
        << "  --case FILE                   Process one case filename from the source folder\n"
        << "  --help                        Show this help\n\n"
        << "Pack input: mapped JSON. Legacy input: .txt, .md, .csv, .tsv, .json, .xml, .html, .htm, .eml.\n"
        << "Binary/PDF/image/Office input is quarantined pending an extraction adapter.\n";
}
std::string next_arg(int& i, int argc, char** argv, const std::string& option) {
    if (++i >= argc) throw std::runtime_error("missing value for " + option);
    return argv[i];
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "--help") {
            usage();
            return argc < 2 ? 2 : 0;
        }
        if (std::string(argv[1]) == "export-analytics") {
            std::string output_dir = "Output";
            std::string analytics_dir;
            for (int i = 2; i < argc; ++i) {
                const std::string arg = argv[i];
                if (arg == "--output") output_dir = next_arg(i, argc, argv, arg);
                else if (arg == "--analytics-dir") analytics_dir = next_arg(i, argc, argv, arg);
                else if (arg == "--help") { usage(); return 0; }
                else throw std::runtime_error("unknown option: " + arg);
            }
            if (analytics_dir.empty()) analytics_dir = (std::filesystem::path(output_dir) / "Analytics").string();
            const auto summary = rag::export_analytics(output_dir, analytics_dir);
            std::cout << "Analytics export: " << summary.csv_file.string() << " ("
                      << summary.cases << " cases, " << summary.findings << " findings)\n";
            return 0;
        }
        if (std::string(argv[1]) == "validate-pack") {
            std::filesystem::path pack_dir;
            for (int i = 2; i < argc; ++i) {
                const std::string arg = argv[i];
                if (arg == "--pack") pack_dir = next_arg(i, argc, argv, arg);
                else if (arg == "--help") { usage(); return 0; }
                else throw std::runtime_error("unknown option: " + arg);
            }
            if (pack_dir.empty()) throw std::runtime_error("validate-pack requires --pack FOLDER");
            rag::validate_pack(pack_dir);
            std::cout << "Pack valid: " << pack_dir.string() << "\n";
            return 0;
        }
        if (std::string(argv[1]) != "run") {
            usage();
            return 2;
        }

        rag::Config config;
        config.use_hermes = true;
        for (int i = 2; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--pack") config.pack_dir = next_arg(i, argc, argv, arg);
            else if (arg == "--source") config.source_dir = next_arg(i, argc, argv, arg);
            else if (arg == "--rules") config.rules_dir = next_arg(i, argc, argv, arg);
            else if (arg == "--output") config.output_dir = next_arg(i, argc, argv, arg);
            else if (arg == "--effective-date") config.effective_date = next_arg(i, argc, argv, arg);
            else if (arg == "--threshold") config.retrieval_threshold = std::stod(next_arg(i, argc, argv, arg));
            else if (arg == "--write-token-vault") config.write_token_vault = true;
            else if (arg == "--hermes") config.use_hermes = true;
            else if (arg == "--offline") config.use_hermes = false;
            else if (arg == "--hermes-cli") config.hermes_executable = next_arg(i, argc, argv, arg);
            else if (arg == "--replay-response") { config.hermes_response_file = next_arg(i, argc, argv, arg); config.use_hermes = true; }
            else if (arg == "--hermes-timeout") config.hermes_timeout_seconds = static_cast<unsigned>(std::stoul(next_arg(i, argc, argv, arg)));
            else if (arg == "--max-documents") config.max_documents = static_cast<unsigned>(std::stoul(next_arg(i, argc, argv, arg)));
            else if (arg == "--case") config.case_file = next_arg(i, argc, argv, arg);
            else if (arg == "--help") { usage(); return 0; }
            else throw std::runtime_error("unknown option: " + arg);
        }

        if (config.source_dir.empty()) config.source_dir = "Tools/Input";
        if (config.rules_dir.empty() && config.pack_dir.empty()) config.rules_dir = "Policy/rules";
        if (config.output_dir.empty()) config.output_dir = "Output";

        const auto summary = rag::run(config);
        std::cout << "Run complete: " << summary.run_dir.string() << "\n"
                  << "Documents: " << summary.documents_processed << " processed, "
                  << summary.documents_quarantined << " quarantined, "
                  << summary.documents_seen << " seen\n"
                  << "Clauses: " << summary.clauses << ", claims: " << summary.claims
                  << ", findings: " << summary.findings << "\n";
        return summary.documents_quarantined > 0 ? 3 : 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return 1;
    }
}
