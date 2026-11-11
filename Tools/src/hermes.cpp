#include "rag/hermes.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace rag {
namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

Json parse_answer(const std::string& output) {
    // Hermes may wrap a JSON object in a Markdown fence or print session metadata.
    const auto first = output.find('{');
    const auto last = output.rfind('}');
    if (first == std::string::npos || last == std::string::npos || last < first)
        throw std::runtime_error("Hermes did not return a JSON object");
    const auto answer = Json::parse(output.substr(first, last - first + 1));
    if (!answer.is_object()) throw std::runtime_error("Hermes response is not a JSON object");
    return answer;
}

#ifdef _WIN32
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE handle = INVALID_HANDLE_VALUE) : value(handle) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

std::filesystem::path find_hermes(const std::filesystem::path& explicit_path) {
    if (!explicit_path.empty()) return std::filesystem::absolute(explicit_path);
    std::wstring buffer(32768, L'\0');
    const DWORD length = SearchPathW(nullptr, L"hermes.cmd", nullptr,
                                     static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    if (length == 0 || length >= buffer.size())
        throw std::runtime_error("hermes.cmd not found on PATH; set --hermes-cli");
    buffer.resize(length);
    return std::filesystem::path(buffer);
}

HermesResult run_hermes(const std::filesystem::path& explicit_path,
                        unsigned timeout_seconds, const std::string& prompt,
                        const std::filesystem::path& work_dir) {
    HermesResult result;
    const auto executable = find_hermes(explicit_path);
    if (!std::filesystem::is_regular_file(executable))
        throw std::runtime_error("Hermes executable does not exist: " + executable.string());
    std::filesystem::create_directories(work_dir);
    const auto output_path = work_dir / "hermes-stdout.raw.txt";
    const auto error_path = work_dir / "hermes-stderr.raw.txt";
    SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE input_read_raw = INVALID_HANDLE_VALUE;
    HANDLE input_write_raw = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&input_read_raw, &input_write_raw, &inheritable, 0))
        throw std::runtime_error("failed to create Hermes stdin pipe");
    Handle input_read(input_read_raw), input_write(input_write_raw);
    SetHandleInformation(input_write.value, HANDLE_FLAG_INHERIT, 0);
    Handle output(CreateFileW(output_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inheritable,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    Handle error(CreateFileW(error_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inheritable,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (output.value == INVALID_HANDLE_VALUE || error.value == INVALID_HANDLE_VALUE)
        throw std::runtime_error("failed to create Hermes output files");

    const std::wstring command_path = executable.wstring();
    if (command_path.find_first_of(L"\"%!^\r\n") != std::wstring::npos)
        throw std::runtime_error("Hermes executable path contains unsafe command characters");
    wchar_t system_dir[MAX_PATH];
    if (!GetSystemDirectoryW(system_dir, MAX_PATH)) throw std::runtime_error("cannot find system directory");
    const std::wstring cmd_exe = std::wstring(system_dir) + L"\\cmd.exe";
    std::wstring command = L"\"" + cmd_exe + L"\" /d /s /c \"\"" + command_path +
        L"\" chat --query-file - --oneshot --quiet --format text --source tool "
        L"--provider llamacpp --reasoning low --ignore-rules -t none --max-turns 1 --run-budget " +
        std::to_wstring(timeout_seconds) + L"\"";
    std::vector<wchar_t> command_buffer(command.begin(), command.end());
    command_buffer.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input_read.value;
    startup.hStdOutput = output.value;
    startup.hStdError = error.value;
    PROCESS_INFORMATION process{};
    const auto cwd = std::filesystem::absolute(work_dir).wstring();
    Handle job(CreateJobObjectW(nullptr, nullptr));
    if (job.value != INVALID_HANDLE_VALUE) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    }
    if (!CreateProcessW(cmd_exe.c_str(), command_buffer.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, cwd.c_str(), &startup, &process))
        throw std::runtime_error("failed to launch Hermes CLI");
    Handle process_handle(process.hProcess), thread_handle(process.hThread);
    if (job.value != INVALID_HANDLE_VALUE) AssignProcessToJobObject(job.value, process_handle.value);
    ResumeThread(thread_handle.value);
    CloseHandle(input_read.value);
    input_read.value = INVALID_HANDLE_VALUE;

    DWORD written = 0;
    if (prompt.size() > 1024 * 1024 ||
        !WriteFile(input_write.value, prompt.data(), static_cast<DWORD>(prompt.size()), &written, nullptr) ||
        written != prompt.size()) {
        TerminateProcess(process_handle.value, 1);
        throw std::runtime_error("failed to send filtered prompt to Hermes");
    }
    CloseHandle(input_write.value);
    input_write.value = INVALID_HANDLE_VALUE;
    const DWORD wait = WaitForSingleObject(process_handle.value, timeout_seconds * 1000);
    if (wait != WAIT_OBJECT_0) {
        TerminateProcess(process_handle.value, 1);
        if (job.value != INVALID_HANDLE_VALUE) { CloseHandle(job.value); job.value = INVALID_HANDLE_VALUE; }
        CloseHandle(output.value); output.value = INVALID_HANDLE_VALUE;
        CloseHandle(error.value); error.value = INVALID_HANDLE_VALUE;
        std::error_code ignored;
        std::filesystem::remove(output_path, ignored);
        std::filesystem::remove(error_path, ignored);
        result.error = "Hermes timed out";
        return result;
    }
    DWORD exit_code = 0;
    GetExitCodeProcess(process_handle.value, &exit_code);
    if (job.value != INVALID_HANDLE_VALUE) {
        CloseHandle(job.value);
        job.value = INVALID_HANDLE_VALUE;
    }
    CloseHandle(output.value); output.value = INVALID_HANDLE_VALUE;
    CloseHandle(error.value); error.value = INVALID_HANDLE_VALUE;
    if (exit_code != 0) {
        result.error = "Hermes exited with code " + std::to_string(exit_code);
        std::error_code ignored;
        std::filesystem::remove(output_path, ignored);
        std::filesystem::remove(error_path, ignored);
        return result;
    }
    try {
        result.answer = parse_answer(read_file(output_path));
        result.ok = true;
    } catch (const std::exception& issue) { result.error = issue.what(); }
    std::error_code ignored;
    std::filesystem::remove(output_path, ignored);
    std::filesystem::remove(error_path, ignored);
    return result;
}
#endif

} // namespace

HermesClient::HermesClient(std::filesystem::path executable, unsigned timeout_seconds)
    : executable_(std::move(executable)), timeout_seconds_(timeout_seconds) {}

HermesResult HermesClient::ask(const std::string& filtered_prompt,
                               const std::filesystem::path& work_dir) const {
#ifdef _WIN32
    return run_hermes(executable_, timeout_seconds_, filtered_prompt, work_dir);
#else
    (void)filtered_prompt;
    (void)work_dir;
    return {false, {}, "Hermes CLI adapter currently supports Windows"};
#endif
}

} // namespace rag
