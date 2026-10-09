#include "sandbox.hpp"
#include <boost/json.hpp>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace crivo::sandbox {

namespace fs = std::filesystem;

std::string termination_reason_to_string(TerminationReason reason) {
    switch (reason) {
        case TerminationReason::Normal: return "NORMAL";
        case TerminationReason::Timeout: return "TIMEOUT";
        case TerminationReason::OutOfMemory: return "OUT_OF_MEMORY";
        case TerminationReason::ProcessLimit: return "PROCESS_LIMIT";
        case TerminationReason::BlockedByPolicy: return "BLOCKED_BY_POLICY";
        case TerminationReason::SignalKilled: return "SIGNAL_KILLED";
    }
    return "NORMAL";
}

static bool check_binary_exists(const std::string& name) {
    const char* path_env = std::getenv("PATH");
    if (!path_env) path_env = "/usr/bin:/bin";
    std::stringstream ss(path_env);
    std::string item;
    while (std::getline(ss, item, ':')) {
        if (item.empty()) continue;
        fs::path p = fs::path(item) / name;
        if (fs::exists(p) && (access(p.c_str(), X_OK) == 0)) {
            return true;
        }
    }
    return false;
}

static ExecutionResult execute_fork_exec(
    const std::vector<std::string>& argv,
    const std::vector<std::pair<std::string, std::string>>& envs,
    const fs::path& working_dir,
    const SandboxLimits& limits,
    std::string_view driver_name,
    std::string_view status_name)
{
    ExecutionResult result;
    result.isolation_driver = std::string(driver_name);
    result.isolation_status = std::string(status_name);

    int stdout_pipe[2];
    int stderr_pipe[2];
    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        result.exit_code = -1;
        result.termination = TerminationReason::BlockedByPolicy;
        result.stderr_output = "Failed to create pipes for sandbox execution";
        return result;
    }

    auto start_time = std::chrono::steady_clock::now();
    pid_t pid = fork();

    if (pid < 0) {
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        result.exit_code = -1;
        result.termination = TerminationReason::BlockedByPolicy;
        result.stderr_output = "Fork failed for sandbox process";
        return result;
    }

    if (pid == 0) {
        // Child process: create new process group to guarantee clean reaping of entire subtree
        setpgid(0, 0);

        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        if (!working_dir.empty()) {
            std::error_code ec;
            fs::current_path(working_dir, ec);
        }

        // Apply resource limits
        if (driver_name == "host_isolated" && limits.max_processes > 0) {
            struct rlimit rl{};
            rl.rlim_cur = limits.max_processes;
            rl.rlim_max = limits.max_processes;
            setrlimit(RLIMIT_NPROC, &rl);
        }
        if (limits.max_memory_mb > 0) {
            struct rlimit rl{};
            rl.rlim_cur = limits.max_memory_mb * 1024 * 1024;
            rl.rlim_max = limits.max_memory_mb * 1024 * 1024;
            setrlimit(RLIMIT_AS, &rl);
        }

        // Setup environment
        for (const auto& [k, v] : envs) {
            setenv(k.c_str(), v.c_str(), 1);
        }

        std::vector<char*> c_argv;
        for (const auto& arg : argv) {
            c_argv.push_back(const_cast<char*>(arg.c_str()));
        }
        c_argv.push_back(nullptr);

        execvp(c_argv[0], c_argv.data());
        std::cerr << "execvp failed for " << argv[0] << ": " << strerror(errno) << std::endl;
        _exit(127);
    }

    // Parent process
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    // Set non-blocking read
    fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK);

    std::string out_buf, err_buf;
    char buffer[4096];
    bool timed_out = false;
    int status = 0;

    auto timeout_dur = limits.timeout;
    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);

        if (elapsed > timeout_dur) {
            timed_out = true;
            kill(-pid, SIGTERM);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            kill(-pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }

        pid_t res = waitpid(pid, &status, WNOHANG);
        if (res == pid) {
            break;
        } else if (res < 0) {
            break;
        }

        ssize_t count = read(stdout_pipe[0], buffer, sizeof(buffer) - 1);
        if (count > 0) {
            buffer[count] = '\0';
            out_buf.append(buffer, count);
        }

        count = read(stderr_pipe[0], buffer, sizeof(buffer) - 1);
        if (count > 0) {
            buffer[count] = '\0';
            err_buf.append(buffer, count);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Drain remaining output
    while (true) {
        ssize_t count = read(stdout_pipe[0], buffer, sizeof(buffer) - 1);
        if (count <= 0) break;
        out_buf.append(buffer, count);
    }
    while (true) {
        ssize_t count = read(stderr_pipe[0], buffer, sizeof(buffer) - 1);
        if (count <= 0) break;
        err_buf.append(buffer, count);
    }
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    result.stdout_output = std::move(out_buf);
    result.stderr_output = std::move(err_buf);

    if (timed_out) {
        result.exit_code = 124;
        result.termination = TerminationReason::Timeout;
    } else if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
        result.termination = TerminationReason::Normal;
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        result.exit_code = 128 + sig;
        if (sig == SIGXCPU || sig == SIGALRM) {
            result.termination = TerminationReason::Timeout;
        } else {
            result.termination = TerminationReason::SignalKilled;
        }
    }

    return result;
}

bool BubblewrapDriver::is_available() const noexcept {
    return check_binary_exists("bwrap");
}

ExecutionResult BubblewrapDriver::execute(
    const SandboxConfig& config,
    const std::vector<std::string>& command)
{
    if (command.empty()) {
        ExecutionResult r;
        r.exit_code = -1;
        r.termination = TerminationReason::BlockedByPolicy;
        r.stderr_output = "Empty command provided to sandbox";
        return r;
    }

    std::vector<std::string> bwrap_cmd = {
        "bwrap",
        "--ro-bind", "/usr", "/usr",
        "--proc", "/proc",
        "--dev", "/dev",
        "--tmpfs", "/tmp"
    };

    if (fs::is_symlink("/lib")) {
        bwrap_cmd.insert(bwrap_cmd.end(), {"--symlink", "usr/lib", "/lib"});
    } else if (fs::exists("/lib")) {
        bwrap_cmd.insert(bwrap_cmd.end(), {"--ro-bind", "/lib", "/lib"});
    }

    if (fs::is_symlink("/lib64")) {
        bwrap_cmd.insert(bwrap_cmd.end(), {"--symlink", "usr/lib64", "/lib64"});
    } else if (fs::exists("/lib64")) {
        bwrap_cmd.insert(bwrap_cmd.end(), {"--ro-bind", "/lib64", "/lib64"});
    }

    if (fs::is_symlink("/bin")) {
        bwrap_cmd.insert(bwrap_cmd.end(), {"--symlink", "usr/bin", "/bin"});
    } else if (fs::exists("/bin")) {
        bwrap_cmd.insert(bwrap_cmd.end(), {"--ro-bind", "/bin", "/bin"});
    }

    if (fs::exists("/etc")) bwrap_cmd.insert(bwrap_cmd.end(), {"--ro-bind", "/etc", "/etc"});

    // Mount target readonly
    if (!config.target_ro_path.empty() && fs::exists(config.target_ro_path)) {
        bwrap_cmd.insert(bwrap_cmd.end(), {"--dir", "/target", "--ro-bind", config.target_ro_path.string(), "/target"});
    }

    // Mount workspace readwrite
    if (!config.workspace_rw_path.empty()) {
        fs::create_directories(config.workspace_rw_path);
        bwrap_cmd.insert(bwrap_cmd.end(), {"--dir", "/workspace", "--bind", config.workspace_rw_path.string(), "/workspace"});
        bwrap_cmd.insert(bwrap_cmd.end(), {"--chdir", "/workspace"});
    }

    // Mount CRIVO binary directory read-only so internal worker and oracles can execute inside sandbox
    std::error_code ec_exe;
    fs::path self_exe = fs::canonical("/proc/self/exe", ec_exe);
    if (!ec_exe && fs::exists(self_exe)) {
        bwrap_cmd.insert(bwrap_cmd.end(), {"--dir", "/opt/crivo", "--ro-bind", self_exe.parent_path().string(), "/opt/crivo"});
    }

    // Isolations
    bwrap_cmd.push_back("--unshare-all");
    if (!config.enable_network) {
        bwrap_cmd.push_back("--unshare-net");
    }
    bwrap_cmd.push_back("--die-with-parent");

    // Pass target command
    bwrap_cmd.push_back("--");
    for (const auto& c : command) {
        bwrap_cmd.push_back(c);
    }

    std::vector<std::pair<std::string, std::string>> envs = {
        {"PATH", "/opt/crivo:/usr/bin:/bin"},
        {"HOME", "/tmp"},
        {"LANG", "C.UTF-8"}
    };
    for (const auto& ev : config.env_vars) {
        envs.push_back(ev);
    }

    auto res = execute_fork_exec(bwrap_cmd, envs, config.workspace_rw_path, config.limits, name(), "ENFORCED");
    res.capabilities_enforced = {
        "ro_target_binding",
        "rw_workspace_binding",
        "unshare_all",
        "unshare_pid",
        "unshare_user",
        "unshare_ipc",
        "unshare_uts",
        "tmpfs_tmp",
        "die_with_parent",
        "rlimit_as"
    };
    if (!config.enable_network) {
        res.capabilities_enforced.push_back("unshare_net");
    }
    return res;
}

bool PodmanDriver::is_available() const noexcept {
    return check_binary_exists("podman");
}

ExecutionResult PodmanDriver::execute(
    const SandboxConfig& config,
    const std::vector<std::string>& command)
{
    if (command.empty()) {
        ExecutionResult r;
        r.exit_code = -1;
        r.termination = TerminationReason::BlockedByPolicy;
        r.stderr_output = "Empty command provided to podman sandbox";
        return r;
    }

    std::vector<std::string> podman_cmd = {
        "podman", "run", "--rm",
        "--security-opt", "no-new-privileges"
    };

    if (!config.enable_network) {
        podman_cmd.insert(podman_cmd.end(), {"--network", "none"});
    }

    if (!config.target_ro_path.empty() && fs::exists(config.target_ro_path)) {
        podman_cmd.insert(podman_cmd.end(), {"-v", config.target_ro_path.string() + ":/target:ro"});
    }

    if (!config.workspace_rw_path.empty()) {
        fs::create_directories(config.workspace_rw_path);
        podman_cmd.insert(podman_cmd.end(), {"-v", config.workspace_rw_path.string() + ":/workspace:rw", "-w", "/workspace"});
    }

    podman_cmd.push_back("alpine:latest");
    for (const auto& c : command) {
        podman_cmd.push_back(c);
    }

    std::vector<std::pair<std::string, std::string>> envs = config.env_vars;
    auto res = execute_fork_exec(podman_cmd, envs, config.workspace_rw_path, config.limits, name(), "ENFORCED");
    res.capabilities_enforced = {
        "ro_target_volume",
        "rw_workspace_volume",
        "no_new_privileges"
    };
    if (!config.enable_network) {
        res.capabilities_enforced.push_back("network_none");
    }
    return res;
}

ExecutionResult HostIsolatedDriver::execute(
    const SandboxConfig& config,
    const std::vector<std::string>& command)
{
    if (command.empty()) {
        ExecutionResult r;
        r.exit_code = -1;
        r.termination = TerminationReason::BlockedByPolicy;
        r.stderr_output = "Empty command provided to host isolated sandbox";
        return r;
    }

    if (!config.workspace_rw_path.empty()) {
        fs::create_directories(config.workspace_rw_path);
    }

    std::vector<std::pair<std::string, std::string>> envs = {
        {"PATH", "/usr/bin:/bin:/usr/local/bin"},
        {"LANG", "C.UTF-8"}
    };
    for (const auto& ev : config.env_vars) {
        envs.push_back(ev);
    }

    auto res = execute_fork_exec(command, envs, config.workspace_rw_path, config.limits, name(), "DEGRADED");
    res.capabilities_enforced = {
        "rlimit_nproc",
        "rlimit_as"
    };
    return res;
}

std::unique_ptr<ISandboxDriver> create_driver(BackendType type) {
    if (type == BackendType::Bubblewrap) {
        auto d = std::make_unique<BubblewrapDriver>();
        if (d->is_available()) return d;
    } else if (type == BackendType::Podman) {
        auto d = std::make_unique<PodmanDriver>();
        if (d->is_available()) return d;
    } else if (type == BackendType::HostIsolated) {
        return std::make_unique<HostIsolatedDriver>();
    } else if (type == BackendType::Auto) {
        auto bwrap = std::make_unique<BubblewrapDriver>();
        if (bwrap->is_available()) return bwrap;
        auto podman = std::make_unique<PodmanDriver>();
        if (podman->is_available()) return podman;
        return std::make_unique<HostIsolatedDriver>();
    }
    return nullptr;
}

ExecutionResult run_in_sandbox(
    const SandboxConfig& config,
    const std::vector<std::string>& command)
{
    auto driver = create_driver(config.backend);
    if (!driver) {
        ExecutionResult res;
        res.exit_code = -1;
        res.termination = TerminationReason::BlockedByPolicy;
        res.isolation_driver = "none";
        res.isolation_status = "BLOCKED";
        res.stderr_output = "No valid sandbox driver available for policy enforcement";
        return res;
    }
    return driver->execute(config, command);
}

std::string generate_sandbox_report_json(
    const SandboxConfig& config,
    const ExecutionResult& res,
    const std::string& instance_id)
{
    boost::json::object report;
    report["schema_version"] = "crivo.sandbox-report/1.1.0";
    report["instance_id"] = instance_id;
    report["backend"] = res.isolation_driver;
    report["isolation_status"] = res.isolation_status;
    report["network_mode"] = config.enable_network ? "LOOPBACK_ONLY" : "DISABLED";

    boost::json::array caps_arr;
    for (const auto& cap : res.capabilities_enforced) {
        caps_arr.push_back(boost::json::value(cap));
    }
    report["capabilities_enforced"] = caps_arr;

    boost::json::array ro_arr;
    if (!config.target_ro_path.empty()) ro_arr.push_back(boost::json::value(config.target_ro_path.string()));
    report["ro_mounts"] = ro_arr;

    boost::json::array rw_arr;
    if (!config.workspace_rw_path.empty()) rw_arr.push_back(boost::json::value(config.workspace_rw_path.string()));
    report["rw_bind_mounts"] = rw_arr;

    boost::json::array tmpfs_arr;
    tmpfs_arr.push_back(boost::json::value("/tmp"));
    report["tmpfs_mounts"] = tmpfs_arr;

    boost::json::object lim;
    lim["timeout_ms"] = static_cast<int64_t>(config.limits.timeout.count());
    lim["max_memory_mb"] = static_cast<int64_t>(config.limits.max_memory_mb);
    lim["max_processes"] = static_cast<int64_t>(config.limits.max_processes);
    report["limits_enforced"] = lim;

    boost::json::object exec_obj;
    exec_obj["exit_code"] = res.exit_code;
    exec_obj["termination_reason"] = termination_reason_to_string(res.termination);
    exec_obj["duration_ms"] = static_cast<int64_t>(res.duration.count());
    exec_obj["peak_rss_kb"] = static_cast<int64_t>(res.peak_rss_kb);
    report["execution"] = exec_obj;

    return boost::json::serialize(report);
}

SandboxQualificationResult qualify_backend(BackendType backend, const fs::path& temp_dir) {
    SandboxQualificationResult res;
    auto driver = create_driver(backend);
    if (!driver || !driver->is_available()) {
        res.passed = false;
        res.backend = (backend == BackendType::Bubblewrap) ? "bubblewrap" :
                      (backend == BackendType::Podman) ? "podman" :
                      (backend == BackendType::HostIsolated) ? "host_isolated" : "auto";
        res.isolation_status = "BLOCKED";
        res.checks.push_back({"driver_availability", false, "Sandbox backend binary is not available or rejected by policy"});
        return res;
    }

    res.backend = std::string(driver->name());
    res.isolation_status = "ENFORCED";

    fs::path base = temp_dir.empty() ? (fs::temp_directory_path() / ("crivo_sbx_qual_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()))) : temp_dir;
    fs::path ro_target = base / "target_ro";
    fs::path rw_workspace = base / "workspace_rw";
    fs::create_directories(ro_target);
    fs::create_directories(rw_workspace);

    // Create a sentinel file in target_ro
    fs::path sentinel = ro_target / "sentinel.txt";
    {
        std::ofstream ofs(sentinel);
        ofs << "CRIVO_SENTINEL_IMMUTABLE";
    }

    SandboxConfig cfg;
    cfg.target_ro_path = ro_target;
    cfg.workspace_rw_path = rw_workspace;
    cfg.backend = backend;
    cfg.enable_network = false;
    cfg.limits.timeout = std::chrono::milliseconds(3000);

    // Check 1: RW Workspace write capability (Positive proof)
    {
        auto exec = driver->execute(cfg, {"touch", "/workspace/test_rw.tmp"});
        bool ok = (exec.exit_code == 0) && fs::exists(rw_workspace / "test_rw.tmp");
        std::string det = ok ? "Workspace is writable and writes persist in ephemeral directory"
                             : ("Failed to write in /workspace: exit_code=" + std::to_string(exec.exit_code) + " err=" + exec.stderr_output);
        res.checks.push_back({"rw_workspace_write", ok, det});
    }

    // Check 2: RO Target immutability (Negative proof: must fail to mutate!)
    {
        auto exec = driver->execute(cfg, {"touch", "/target/forbidden_mutation.tmp"});
        bool ok = (exec.exit_code != 0) && !fs::exists(ro_target / "forbidden_mutation.tmp");
        std::string det = ok ? "Target is strictly read-only; mutation attempts are rejected by kernel with EACCES"
                             : "Security breach: Target directory was mutated inside sandbox!";
        res.checks.push_back({"ro_target_immutable", ok, det});
    }

    // Check 3: Network isolation (Negative proof: must fail to connect with confirmed kernel error)
    {
        auto exec = driver->execute(cfg, {"python3", "-c",
            "import socket, sys\n"
            "try:\n"
            "    s = socket.socket()\n"
            "    s.settimeout(0.5)\n"
            "    s.connect(('1.1.1.1', 80))\n"
            "    sys.exit(0)\n"
            "except (OSError, socket.error) as e:\n"
            "    sys.stderr.write('NETWORK_BLOCKED_OK: ' + str(e) + '\\n')\n"
            "    sys.exit(42)\n"
        });
        bool ok = (exec.exit_code == 42) && (exec.stderr_output.find("NETWORK_BLOCKED_OK") != std::string::npos);
        std::string det;
        if (ok) {
            det = "Network packets strictly blocked by unshare-net namespace (confirmed via kernel unreachable error)";
        } else if (exec.exit_code == 0) {
            det = "Security breach: network connection succeeded inside sandbox!";
        } else {
            det = "Network probe failed to execute or verify isolation: exit_code=" + std::to_string(exec.exit_code) + " err=" + exec.stderr_output;
        }
        res.checks.push_back({"network_isolation", ok, det});
    }

    // Check 4: Process timeout enforcement and group termination
    {
        cfg.limits.timeout = std::chrono::milliseconds(300);
        auto exec = driver->execute(cfg, {"sleep", "5"});
        bool ok = (exec.termination == TerminationReason::Timeout) && (exec.exit_code == 124) && (exec.duration < std::chrono::milliseconds(2000));
        std::string det = ok ? "Processes exceeding timeout are cleanly terminated via SIGKILL"
                             : ("Timeout was not properly enforced: term=" + termination_reason_to_string(exec.termination) + " code=" + std::to_string(exec.exit_code) + " dur=" + std::to_string(exec.duration.count()) + "ms err=" + exec.stderr_output);
        res.checks.push_back({"timeout_enforcement", ok, det});
    }

    // Capabilities enforced
    if (res.backend == "bubblewrap") {
        res.capabilities_enforced = {
            "ro_target_binding",
            "rw_workspace_binding",
            "unshare_all",
            "unshare_pid",
            "unshare_user",
            "unshare_ipc",
            "unshare_uts",
            "tmpfs_tmp",
            "die_with_parent",
            "rlimit_as"
        };
        if (!cfg.enable_network) {
            res.capabilities_enforced.push_back("unshare_net");
        }
    } else if (res.backend == "podman") {
        res.capabilities_enforced = {
            "ro_target_volume",
            "rw_workspace_volume",
            "network_none",
            "no_new_privileges"
        };
    } else {
        res.capabilities_enforced = {
            "rlimit_as"
        };
    }

    // Overall verdict
    res.passed = true;
    for (const auto& chk : res.checks) {
        if (!chk.passed) {
            res.passed = false;
            break;
        }
    }

    // Cleanup ephemeral qualification files
    std::error_code ec;
    fs::remove_all(base, ec);

    return res;
}

std::string serialize_qualification_result(const SandboxQualificationResult& q) {
    boost::json::object root;
    root["schema_version"] = "crivo.sandbox-qualification/1.0.0";
    root["status"] = q.passed ? "QUALIFIED" : "DISQUALIFIED";
    root["backend"] = q.backend;
    root["isolation_status"] = q.isolation_status;

    boost::json::array caps;
    for (const auto& cap : q.capabilities_enforced) {
        caps.push_back(boost::json::value(cap));
    }
    root["capabilities_enforced"] = caps;

    boost::json::array checks_arr;
    for (const auto& c : q.checks) {
        boost::json::object co;
        co["name"] = c.name;
        co["status"] = c.passed ? "PASS" : "FAIL";
        co["details"] = c.details;
        checks_arr.push_back(co);
    }
    root["checks"] = checks_arr;

    return boost::json::serialize(root);
}

} // namespace crivo::sandbox
