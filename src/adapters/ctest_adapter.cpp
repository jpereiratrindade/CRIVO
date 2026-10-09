#include "ctest_adapter.hpp"
#include "../registry/lifecycle.hpp"
#include <boost/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <array>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace crivo::adapters {

namespace {

namespace json = boost::json;
namespace fs = std::filesystem;

std::string current_utc_iso8601() {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

struct ProcessResult {
  int exit_code{-1};
  bool timed_out{false};
  std::string stdout_text;
  std::string stderr_text;
};

void drain_fd(int fd, std::string& target) {
  std::array<char, 4096> buffer{};
  while (true) {
    const ssize_t n = read(fd, buffer.data(), buffer.size());
    if (n > 0) target.append(buffer.data(), static_cast<size_t>(n));
    else if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) break;
    else if (errno == EAGAIN || errno == EWOULDBLOCK) break;
  }
}

ProcessResult exec_process(const std::vector<std::string>& args, unsigned int timeout_seconds) {
  ProcessResult result;
  if (args.empty()) return result;

  int stdout_pipe[2];
  int stderr_pipe[2];
  if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
    result.stderr_text = "Falha ao criar pipes do processo";
    return result;
  }

  const pid_t pid = fork();
  if (pid == 0) {
    setpgid(0, 0);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    dup2(stderr_pipe[1], STDERR_FILENO);
    close(stdout_pipe[0]); close(stdout_pipe[1]);
    close(stderr_pipe[0]); close(stderr_pipe[1]);
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }

  close(stdout_pipe[1]);
  close(stderr_pipe[1]);
  if (pid < 0) {
    close(stdout_pipe[0]); close(stderr_pipe[0]);
    result.stderr_text = "Falha ao criar processo";
    return result;
  }
  setpgid(pid, pid);
  fcntl(stdout_pipe[0], F_SETFL, fcntl(stdout_pipe[0], F_GETFL) | O_NONBLOCK);
  fcntl(stderr_pipe[0], F_SETFL, fcntl(stderr_pipe[0], F_GETFL) | O_NONBLOCK);

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
  int status = 0;
  while (waitpid(pid, &status, WNOHANG) == 0) {
    drain_fd(stdout_pipe[0], result.stdout_text);
    drain_fd(stderr_pipe[0], result.stderr_text);
    if (std::chrono::steady_clock::now() >= deadline) {
      kill(-pid, SIGKILL);
      waitpid(pid, &status, 0);
      result.timed_out = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  drain_fd(stdout_pipe[0], result.stdout_text);
  drain_fd(stderr_pipe[0], result.stderr_text);
  close(stdout_pipe[0]); close(stderr_pipe[0]);
  if (result.timed_out) result.exit_code = 124;
  else if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
  else if (WIFSIGNALED(status)) result.exit_code = 128 + WTERMSIG(status);
  return result;
}

bool valid_project_id(const std::string& id) {
  if (id.empty() || id.size() > 128) return false;
  return std::all_of(id.begin(), id.end(), [](unsigned char c) {
    return std::isalnum(c) || c == '.' || c == '_' || c == '-';
  });
}

bool path_within(const fs::path& child, const fs::path& root) {
  auto child_it = child.begin();
  for (auto root_it = root.begin(); root_it != root.end(); ++root_it, ++child_it) {
    if (child_it == child.end() || *child_it != *root_it) return false;
  }
  return true;
}

bool authorize_build_dir(const std::string& build_dir, const std::string& workspace_root,
                         std::string& canonical_build_dir, std::string& error) {
  std::error_code ec;
  const fs::path build = fs::canonical(build_dir, ec);
  if (ec || !fs::is_directory(build)) {
    error = "Diretorio de build nao encontrado: " + build_dir;
    return false;
  }
  const fs::path root = workspace_root.empty() ? build : fs::canonical(workspace_root, ec);
  if (ec || !fs::is_directory(root)) {
    error = "Raiz de workspace nao encontrada: " + workspace_root;
    return false;
  }
  if (!path_within(build, root)) {
    error = "Diretorio de build fora da raiz autorizada: " + build.string();
    return false;
  }
  canonical_build_dir = build.string();
  return true;
}

std::string get_file_content(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return "";
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

} // namespace

CTestDiscoveryResult discover_ctest(const std::string& build_dir,
                                    const std::string& project_id,
                                    const std::string& workspace_root,
                                    unsigned int timeout_seconds) {
  CTestDiscoveryResult res;
  res.project_id = project_id;
  res.build_dir = build_dir;
  res.discovered_at = current_utc_iso8601();

  if (!valid_project_id(project_id)) {
    res.success = false;
    res.error_message = "project_id invalido; use somente letras, numeros, ponto, hifen e sublinhado";
    return res;
  }
  std::string canonical_build_dir;
  if (!authorize_build_dir(build_dir, workspace_root, canonical_build_dir, res.error_message)) return res;
  res.build_dir = canonical_build_dir;

  const auto version_process = exec_process({"ctest", "--version"}, timeout_seconds);
  std::stringstream ss(version_process.stdout_text);
  std::string line;
  if (std::getline(ss, line)) {
    res.ctest_version = line;
  } else {
    res.ctest_version = "ctest unknown";
  }

  const auto process = exec_process(
      {"ctest", "--test-dir", canonical_build_dir, "--show-only=json-v1"}, timeout_seconds);
  const std::string& output = process.stdout_text;

  if (process.exit_code != 0 || output.empty()) {
    res.success = false;
    res.error_message = process.timed_out ? "Timeout na descoberta CTest" :
        "Falha ao executar ctest --show-only=json-v1 (codigo " + std::to_string(process.exit_code) + "): " + process.stderr_text;
    return res;
  }

  res.raw_json = output;

  boost::system::error_code ec;
  json::value jv = json::parse(output, ec);
  if (ec || !jv.is_object()) {
    res.success = false;
    res.error_message = "Saida do CTest nao e JSON valido: " + ec.message();
    return res;
  }

  const auto& obj = jv.as_object();
  auto it_kind = obj.find("kind");
  if (it_kind == obj.end() || it_kind->value() != "ctestInfo") {
    res.success = false;
    res.error_message = "JSON do CTest nao contem kind=ctestInfo valido";
    return res;
  }

  auto it_tests = obj.find("tests");
  if (it_tests != obj.end() && it_tests->value().is_array()) {
    for (const auto& t_val : it_tests->value().as_array()) {
      if (!t_val.is_object()) continue;
      const auto& t_obj = t_val.as_object();
      DiscoveredTest dt;
      auto it_name = t_obj.find("name");
      if (it_name != t_obj.end() && it_name->value().is_string()) {
        dt.name = std::string(it_name->value().as_string());
      }
      auto it_cmd = t_obj.find("command");
      if (it_cmd != t_obj.end() && it_cmd->value().is_array()) {
        for (const auto& c_item : it_cmd->value().as_array()) {
          if (c_item.is_string()) dt.command.push_back(std::string(c_item.as_string()));
        }
      }
      auto it_props = t_obj.find("properties");
      if (it_props != t_obj.end() && it_props->value().is_array()) {
        for (const auto& p_item : it_props->value().as_array()) {
          if (p_item.is_object()) {
            const auto& p_obj = p_item.as_object();
            CTestProperty prop;
            auto it_pn = p_obj.find("name");
            auto it_pv = p_obj.find("value");
            if (it_pn != p_obj.end() && it_pn->value().is_string()) prop.name = std::string(it_pn->value().as_string());
            if (it_pv != p_obj.end() && it_pv->value().is_string()) prop.value = std::string(it_pv->value().as_string());
            dt.properties.push_back(prop);
          }
        }
      }
      res.tests.push_back(dt);
    }
  }

  res.total_discovered = res.tests.size();
  res.success = true;
  return res;
}

std::string serialize_discovery_json(const CTestDiscoveryResult& disc) {
  json::object root;
  root["schema_version"] = disc.schema_version;
  root["project_id"] = disc.project_id;
  root["build_dir"] = disc.build_dir;
  root["discovered_at"] = disc.discovered_at;
  root["ctest_version"] = disc.ctest_version;
  root["total_discovered"] = disc.total_discovered;

  json::array t_arr;
  for (const auto& t : disc.tests) {
    json::object to;
    to["name"] = t.name;
    json::array cmd_arr;
    for (const auto& c : t.command) cmd_arr.push_back(json::string(c));
    to["command"] = cmd_arr;
    json::array prop_arr;
    for (const auto& p : t.properties) {
      json::object po;
      po["name"] = p.name;
      po["value"] = p.value;
      prop_arr.push_back(po);
    }
    to["properties"] = prop_arr;
    t_arr.push_back(to);
  }
  root["tests"] = t_arr;

  return json::serialize(root);
}

CTestExecutionResult run_ctest(const std::string& build_dir,
                               const std::string& evidence_dir,
                               const std::string& project_id,
                               const std::string& workspace_root,
                               unsigned int timeout_seconds) {
  CTestExecutionResult res;
  res.project_id = project_id;
  res.start_utc = current_utc_iso8601();

  if (!valid_project_id(project_id)) {
    res.success = false;
    res.error_message = "project_id invalido; use somente letras, numeros, ponto, hifen e sublinhado";
    return res;
  }
  std::string canonical_build_dir;
  if (!authorize_build_dir(build_dir, workspace_root, canonical_build_dir, res.error_message)) return res;

  fs::path evpath(evidence_dir);
  fs::create_directories(evpath);

  std::string canonical_ev_dir = fs::canonical(evpath).string();
  fs::path junit_file = fs::path(canonical_ev_dir) / "junit.xml";
  std::error_code remove_ec;
  fs::remove(junit_file, remove_ec);

  const auto version_process = exec_process({"ctest", "--version"}, timeout_seconds);
  std::stringstream ss(version_process.stdout_text);
  std::string line;
  if (std::getline(ss, line)) res.ctest_version = line;

  auto t_start = std::chrono::steady_clock::now();
  const auto process = exec_process(
      {"ctest", "--test-dir", canonical_build_dir, "--output-junit", junit_file.string()}, timeout_seconds);
  auto t_end = std::chrono::steady_clock::now();

  res.end_utc = current_utc_iso8601();
  res.duration_seconds = std::chrono::duration<double>(t_end - t_start).count();

  if (process.timed_out) {
    res.error_message = "Timeout na execucao CTest";
    return res;
  }

  res.junit_path = junit_file.string();
  std::string junit_content = get_file_content(junit_file.string());
  if (junit_content.empty()) {
    res.error_message = "CTest nao produziu JUnit; codigo " + std::to_string(process.exit_code) + ": " + process.stderr_text;
    return res;
  }
  res.junit_sha256 = crivo::registry::compute_sha256_hex(junit_content);

  // Parse summary from junit.xml if available
  // Formato standard JUnit: <testsuite name="CTest" tests="N" failures="N" ...>
  size_t total = 0, failures = 0, errors = 0, skipped = 0;
  auto find_attr = [&](const std::string& attr) -> size_t {
    auto pos = junit_content.find(attr + "=\"");
    if (pos == std::string::npos) return 0;
    pos += attr.size() + 2;
    auto end_pos = junit_content.find("\"", pos);
    if (end_pos == std::string::npos) return 0;
    try {
      return std::stoul(junit_content.substr(pos, end_pos - pos));
    } catch (...) { return 0; }
  };

  total = find_attr("tests");
  failures = find_attr("failures");
  errors = find_attr("errors");
  skipped = find_attr("skipped");

  res.total = total;
  res.failed = failures + errors;
  res.skipped = skipped;
  res.passed = (total >= (res.failed + res.skipped)) ? (total - res.failed - res.skipped) : 0;

  res.evidence_id = "evidence-" + project_id + "-" + res.start_utc;

  // Gerar evidence.json
  json::object ev_obj;
  ev_obj["schema_version"] = res.schema_version;
  ev_obj["evidence_id"] = res.evidence_id;
  ev_obj["project_id"] = res.project_id;

  json::object exec_o;
  exec_o["adapter"] = "ctest";
  exec_o["version"] = res.ctest_version;
  ev_obj["executor"] = exec_o;

  json::object timing_o;
  timing_o["start_utc"] = res.start_utc;
  timing_o["end_utc"] = res.end_utc;
  timing_o["duration_seconds"] = res.duration_seconds;
  ev_obj["timing"] = timing_o;

  json::object sum_o;
  sum_o["total"] = static_cast<int64_t>(res.total);
  sum_o["passed"] = static_cast<int64_t>(res.passed);
  sum_o["failed"] = static_cast<int64_t>(res.failed);
  sum_o["skipped"] = static_cast<int64_t>(res.skipped);
  ev_obj["summary"] = sum_o;

  json::array art_arr;
  json::object j_art;
  j_art["path"] = "junit.xml";
  j_art["format"] = "junit_xml";
  j_art["sha256"] = res.junit_sha256;
  art_arr.push_back(j_art);
  ev_obj["artifacts"] = art_arr;

  res.evidence_json_content = json::serialize(ev_obj);
  fs::path ev_json_path = fs::path(canonical_ev_dir) / "evidence.json";
  std::ofstream out_ev(ev_json_path);
  out_ev << res.evidence_json_content << "\n";
  res.evidence_json_path = ev_json_path.string();

  res.success = true;
  return res;
}

} // namespace crivo::adapters
