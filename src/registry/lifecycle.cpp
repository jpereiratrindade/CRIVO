#include "lifecycle.hpp"
#include <boost/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <iostream>

namespace crivo::registry {

namespace {

namespace json = boost::json;

// Implementação canônica compacta de SHA-256 (FIPS 180-4)
class Sha256 {
public:
  Sha256() { reset(); }

  void reset() {
    state_[0] = 0x6a09e667;
    state_[1] = 0xbb67ae85;
    state_[2] = 0x3c6ef372;
    state_[3] = 0xa54ff53a;
    state_[4] = 0x510e527f;
    state_[5] = 0x9b05688c;
    state_[6] = 0x1f83d9ab;
    state_[7] = 0x5be0cd19;
    count_ = 0;
  }

  void update(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      buffer_[count_ & 63] = data[i];
      count_++;
      if ((count_ & 63) == 0) {
        transform(buffer_);
      }
    }
  }

  void update(const std::string& str) {
    update(reinterpret_cast<const uint8_t*>(str.data()), str.size());
  }

  std::string final_hex() {
    uint64_t total_bits = count_ * 8;
    uint8_t pad = 0x80;
    update(&pad, 1);
    while ((count_ & 63) != 56) {
      uint8_t zero = 0;
      update(&zero, 1);
    }
    for (int i = 7; i >= 0; --i) {
      uint8_t b = static_cast<uint8_t>((total_bits >> (i * 8)) & 0xff);
      update(&b, 1);
    }
    std::stringstream ss;
    for (int i = 0; i < 8; ++i) {
      ss << std::hex << std::setfill('0') << std::setw(8) << state_[i];
    }
    return ss.str();
  }

private:
  uint32_t state_[8];
  uint64_t count_;
  uint8_t buffer_[64];

  static inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
  static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
  static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
  static inline uint32_t sig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
  static inline uint32_t sig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
  static inline uint32_t gam0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
  static inline uint32_t gam1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

  void transform(const uint8_t* block) {
    static const uint32_t K[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = (block[i * 4] << 24) | (block[i * 4 + 1] << 16) | (block[i * 4 + 2] << 8) | (block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
      w[i] = gam1(w[i - 2]) + w[i - 7] + gam0(w[i - 15]) + w[i - 16];
    }

    uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

    for (int i = 0; i < 64; ++i) {
      uint32_t t1 = h + sig1(e) + ch(e, f, g) + K[i] + w[i];
      uint32_t t2 = sig0(a) + maj(a, b, c);
      h = g; g = f; f = e; e = d + t1;
      d = c; c = b; b = a; a = t1 + t2;
    }

    state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
  }
};

std::string current_utc_iso8601() {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

void ensure_lifecycle_schema(sqlite3* db) {
  const char* ddl = R"(
    CREATE TABLE IF NOT EXISTS lifecycle_events (
      event_id TEXT PRIMARY KEY,
      entity_type TEXT NOT NULL,
      entity_ref TEXT NOT NULL,
      event_type TEXT NOT NULL,
      occurred_at TEXT NOT NULL,
      recorded_at TEXT NOT NULL,
      cause TEXT NOT NULL,
      authority_ref TEXT NOT NULL
    );
  )";
  sqlite3_exec(db, ddl, nullptr, nullptr, nullptr);
}

} // namespace

std::string compute_sha256_hex(const std::string& input) {
  Sha256 sha;
  sha.update(input);
  return sha.final_hex();
}

bool record_lifecycle_event(sqlite3* db, LifecycleEvent& event) {
  ensure_lifecycle_schema(db);
  std::string now = current_utc_iso8601();
  if (event.occurred_at.empty()) event.occurred_at = now;
  if (event.recorded_at.empty()) event.recorded_at = now;
  if (event.event_id.empty()) {
    event.event_id = "evt-" + event.entity_type + "-" + event.event_type + "-" + now;
  }

  // Canonical payload representation for digest calculation
  std::string canonical_payload = event.entity_type + "|" + event.entity_ref + "|" +
                                  event.event_type + "|" + event.occurred_at + "|" +
                                  event.cause + "|" + event.authority_ref;
  event.payload_digest_sha256 = compute_sha256_hex(canonical_payload);

  const char* sql = R"(
    INSERT OR IGNORE INTO lifecycle_events
    (event_id, entity_type, entity_ref, event_type, occurred_at, recorded_at, cause, authority_ref)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?);
  )";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

  sqlite3_bind_text(stmt, 1, event.event_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, event.entity_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, event.entity_ref.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, event.event_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, event.occurred_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, event.recorded_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, event.cause.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, event.authority_ref.c_str(), -1, SQLITE_TRANSIENT);

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return (rc == SQLITE_DONE);
}

std::vector<LifecycleEvent> list_lifecycle_events(sqlite3* db,
                                                  const std::string& entity_ref_filter,
                                                  const std::string& event_type_filter) {
  ensure_lifecycle_schema(db);
  std::vector<LifecycleEvent> results;
  std::string sql = "SELECT event_id, entity_type, entity_ref, event_type, occurred_at, recorded_at, cause, authority_ref FROM lifecycle_events WHERE 1=1";
  if (!entity_ref_filter.empty()) sql += " AND entity_ref = ?";
  if (!event_type_filter.empty()) sql += " AND event_type = ?";
  sql += " ORDER BY recorded_at ASC;";

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return results;

  int bind_idx = 1;
  if (!entity_ref_filter.empty()) sqlite3_bind_text(stmt, bind_idx++, entity_ref_filter.c_str(), -1, SQLITE_TRANSIENT);
  if (!event_type_filter.empty()) sqlite3_bind_text(stmt, bind_idx++, event_type_filter.c_str(), -1, SQLITE_TRANSIENT);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    LifecycleEvent ev;
    ev.event_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    ev.entity_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    ev.entity_ref = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    ev.event_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    ev.occurred_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    ev.recorded_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    ev.cause = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    ev.authority_ref = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));

    std::string canonical_payload = ev.entity_type + "|" + ev.entity_ref + "|" +
                                    ev.event_type + "|" + ev.occurred_at + "|" +
                                    ev.cause + "|" + ev.authority_ref;
    ev.payload_digest_sha256 = compute_sha256_hex(canonical_payload);
    results.push_back(ev);
  }
  sqlite3_finalize(stmt);
  return results;
}

std::string serialize_events_json(const std::vector<LifecycleEvent>& events) {
  json::array arr;
  for (const auto& ev : events) {
    json::object o;
    o["schema_version"] = ev.schema_version;
    o["event_id"] = ev.event_id;
    o["entity_type"] = ev.entity_type;
    o["entity_ref"] = ev.entity_ref;
    o["event_type"] = ev.event_type;
    o["occurred_at"] = ev.occurred_at;
    o["recorded_at"] = ev.recorded_at;
    o["cause"] = ev.cause;
    o["authority_ref"] = ev.authority_ref;
    o["payload_digest_sha256"] = ev.payload_digest_sha256;
    arr.push_back(o);
  }
  return json::serialize(arr);
}

size_t reconcile_interrupted_runs(sqlite3* db, const std::string& authority_ref) {
  ensure_lifecycle_schema(db);
  // Localiza runs em estado RUNNING órfão
  std::vector<std::string> orphan_run_ids;
  const char* sql = "SELECT run_id FROM runs WHERE status = 'RUNNING';";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      orphan_run_ids.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
  }

  size_t count = 0;
  for (const auto& r_id : orphan_run_ids) {
    LifecycleEvent ev;
    ev.entity_type = "test_execution";
    ev.entity_ref = r_id;
    ev.event_type = "RUN_INTERRUPTED";
    ev.cause = "unclean_shutdown_reconciliation";
    ev.authority_ref = authority_ref;
    if (record_lifecycle_event(db, ev)) {
      count++;
    }
  }
  return count;
}

} // namespace crivo::registry
