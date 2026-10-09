#pragma once
#include <string>
#include <vector>
#include <optional>
#include <sqlite3.h>

namespace crivo::registry {

struct LifecycleEvent {
  std::string schema_version{"crivo.lifecycle-event/1.0.0"};
  std::string event_id;
  std::string entity_type;
  std::string entity_ref;
  std::string event_type;
  std::string occurred_at;
  std::string recorded_at;
  std::string cause;
  std::string authority_ref;
  std::optional<std::string> baseline_ref;
  std::vector<std::string> evidence_refs;
  std::optional<std::string> predecessor_event_id;
  std::string payload_digest_sha256;
};

std::string compute_sha256_hex(const std::string& input);

bool record_lifecycle_event(sqlite3* db, LifecycleEvent& event);

std::vector<LifecycleEvent> list_lifecycle_events(sqlite3* db,
                                                  const std::string& entity_ref_filter = "",
                                                  const std::string& event_type_filter = "");

std::string serialize_events_json(const std::vector<LifecycleEvent>& events);

size_t reconcile_interrupted_runs(sqlite3* db, const std::string& authority_ref = "crivo-reconciliation");

} // namespace crivo::registry
