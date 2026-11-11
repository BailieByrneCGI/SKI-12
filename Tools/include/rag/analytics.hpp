#pragma once

#include <filesystem>

namespace rag {

struct AnalyticsSummary {
    unsigned cases = 0;
    unsigned findings = 0;
    std::filesystem::path csv_file;
};

// Exports a fixed, text-free subset of the case findings for local analysis.
AnalyticsSummary export_analytics(const std::filesystem::path& cases_dir,
                                  const std::filesystem::path& analytics_dir);

} // namespace rag
