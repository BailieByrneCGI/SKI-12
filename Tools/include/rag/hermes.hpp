#pragma once

#include "rag/json.hpp"

#include <filesystem>
#include <string>

namespace rag {

struct HermesResult {
    bool ok = false;
    Json answer;
    std::string error;
};

class HermesClient {
public:
    explicit HermesClient(std::filesystem::path executable = {}, unsigned timeout_seconds = 300);
    HermesResult ask(const std::string& filtered_prompt, const std::filesystem::path& work_dir) const;

private:
    std::filesystem::path executable_;
    unsigned timeout_seconds_;
};

} // namespace rag
