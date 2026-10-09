#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace crivo::sandbox {

enum class BackendType {
    Auto,
    Bubblewrap,
    Podman,
    HostIsolated
};

enum class TerminationReason {
    Normal,
    Timeout,
    OutOfMemory,
    ProcessLimit,
    BlockedByPolicy,
    SignalKilled
};

struct SandboxLimits {
    std::chrono::milliseconds timeout{30000};
    uint64_t max_memory_mb{1024};
    uint32_t max_processes{64};
};

struct SandboxConfig {
    std::filesystem::path target_ro_path;
    std::filesystem::path workspace_rw_path;
    std::filesystem::path evidence_dir;
    bool enable_network{false};
    BackendType backend{BackendType::Auto};
    SandboxLimits limits;
    std::vector<std::pair<std::string, std::string>> env_vars;
};

struct ExecutionResult {
    int exit_code{-1};
    TerminationReason termination{TerminationReason::Normal};
    std::chrono::milliseconds duration{0};
    uint64_t peak_rss_kb{0};
    std::string stdout_output;
    std::string stderr_output;
    std::string isolation_driver;
    std::string isolation_status; // "ENFORCED", "DEGRADED", "BLOCKED"
};

class ISandboxDriver {
public:
    virtual ~ISandboxDriver() = default;
    virtual bool is_available() const noexcept = 0;
    virtual std::string_view name() const noexcept = 0;
    virtual ExecutionResult execute(const SandboxConfig& config,
                                   const std::vector<std::string>& command) = 0;
};

class BubblewrapDriver final : public ISandboxDriver {
public:
    bool is_available() const noexcept override;
    std::string_view name() const noexcept override { return "bubblewrap"; }
    ExecutionResult execute(const SandboxConfig& config,
                           const std::vector<std::string>& command) override;
};

class PodmanDriver final : public ISandboxDriver {
public:
    bool is_available() const noexcept override;
    std::string_view name() const noexcept override { return "podman"; }
    ExecutionResult execute(const SandboxConfig& config,
                           const std::vector<std::string>& command) override;
};

class HostIsolatedDriver final : public ISandboxDriver {
public:
    bool is_available() const noexcept override { return true; }
    std::string_view name() const noexcept override { return "host_isolated"; }
    ExecutionResult execute(const SandboxConfig& config,
                           const std::vector<std::string>& command) override;
};

std::unique_ptr<ISandboxDriver> create_driver(BackendType type);
ExecutionResult run_in_sandbox(const SandboxConfig& config,
                              const std::vector<std::string>& command);
std::string termination_reason_to_string(TerminationReason reason);
std::string generate_sandbox_report_json(const SandboxConfig& config,
                                        const ExecutionResult& res,
                                        const std::string& instance_id);

} // namespace crivo::sandbox
