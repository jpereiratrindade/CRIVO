#include "core.hpp"
#include "registry/lifecycle.hpp"
#include "memory.hpp"
#include "dev_context.hpp"
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <sqlite3.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
using namespace std::string_literals;
namespace crivo {
namespace {
using Db = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
Db open_db(const std::string& path, bool readonly=false) {
  sqlite3* raw=nullptr;
  const int flags=readonly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE;
  const int code=sqlite3_open_v2(path.c_str(), &raw, flags, nullptr);
  Db db(raw, &sqlite3_close);
  if (code!=SQLITE_OK) throw std::runtime_error("SQLite: "s+(raw?sqlite3_errmsg(raw):"open failure"));
  sqlite3_busy_timeout(db.get(),3000);
  return db;
}
Statement prepare(sqlite3* db, std::string_view sql) {
  sqlite3_stmt* raw=nullptr;
  if(sqlite3_prepare_v2(db,sql.data(),static_cast<int>(sql.size()),&raw,nullptr)!=SQLITE_OK)
    throw std::runtime_error(sqlite3_errmsg(db));
  return {raw,&sqlite3_finalize};
}
void exec(sqlite3* db,const std::string& sql) {
  char* err=nullptr;
  if(sqlite3_exec(db, sql.c_str(),nullptr,nullptr,&err)!=SQLITE_OK) {
    auto message=std::string(err?err:"SQLite exec error"); sqlite3_free(err);
    throw std::runtime_error(message);
  }
}
void bind_text(sqlite3_stmt* st, int index, const std::string& value) {
  if(sqlite3_bind_text(st,index,value.c_str(),-1,SQLITE_TRANSIENT)!=SQLITE_OK)
    throw std::runtime_error("Erro ao vincular parametro SQLite");
}
std::string str(sqlite3_stmt* st,int col) {
  const auto* p=sqlite3_column_text(st,col);
  return p?reinterpret_cast<const char*>(p):"";
}
std::vector<std::string> as_strings(const boost::property_tree::ptree& node) {
  std::vector<std::string> result;
  for (const auto& item : node) result.push_back(item.second.get_value<std::string>());
  return result;
}
std::string json_array(const std::vector<std::string>& a) {
  std::string out="[";
  for (const auto& s : a) { if(out.size()>1) out+=','; out+='"'+json_escape(s)+'"'; }
  return out+"]";
}
std::string json_row(const TestDefinition& x) {
  return "{\"schema_version\":\"crivo.service/0.1.0\",\"id\":\""+
    json_escape(x.id)+"\",\"name\":\""+json_escape(x.name)+
      "\",\"category\":\""+json_escape(x.category)+"\",\"subcategory\":\""+json_escape(x.subcategory)+
      "\",\"purpose\":\""+json_escape(x.purpose)+"\",\"status\":\""+json_escape(x.status)+
      "\",\"engine\":\""+json_escape(x.engine)+"\",\"profiles\":"+json_array(x.profiles)+
      ",\"tags\":"+json_array(x.tags)+"}";
}
std::string service_row(const TestDefinition& x) {
  const bool available=x.status=="implemented";
  return "{\"id\":\""+json_escape(x.id)+"\",\"name\":\""+json_escape(x.name)+
    "\",\"category\":\""+json_escape(x.category)+"\",\"subcategory\":\""+
    json_escape(x.subcategory)+"\",\"purpose\":\""+json_escape(x.purpose)+
    "\",\"availability\":\""+(available?"IMPLEMENTED":"PLANNED")+
    "\",\"consumption\":{\"mode\":\""+(available?"CLI_PROFILE":"NOT_AVAILABLE")+
    "\",\"profiles\":"+json_array(x.profiles)+"},\"output_states\":"+
    (available?"[\"PASS\",\"FAIL\",\"ERROR\"]":"[\"BLOCKED\"]")+
    ",\"authority\":\"NOT_DEFINED\",\"evidence\":\""+
    (available?"LOCAL_SUMMARY":"NONE")+"\","+
    "\"limitations\":[\"No certification claim\",\"Result valid only for declared local execution context\"]}";
}
std::string simple_result(sqlite3* db,const std::string& sql) {
  const auto st=prepare(db,sql);
  if(sqlite3_step(st.get())!=SQLITE_ROW) throw std::runtime_error("Query diagnostica sem linha");
  return str(st.get(),0);
}
std::pair<bool,std::string> execute_builtin(const TestDefinition& def, sqlite3* db,const std::string& file) {
  if(def.id=="catalog.schema") {
    const auto all=parse_catalog(file);
    return {!all.empty(),"Manifesto JSON validado: "+std::to_string(all.size())+" testes"};
  }
  if(def.id=="sqlite.integrity") {
    const auto result=simple_result(db,"PRAGMA quick_check;");
    return {result=="ok", "SQLite quick_check: "+result};
  }
  if(def.id=="sqlite.wal") {
    const auto result=simple_result(db,"PRAGMA journal_mode;");
    return {result=="wal", "journal_mode="+result};
  }
  return {false,"Executor indisponivel para este teste"};
}
void save_run(sqlite3* db,const TestDefinition& test,const std::string& profile,
  const std::string& status,const std::string& detail,long long duration_ms) {
  auto st=prepare(db,"INSERT INTO runs(test_id,category,profile,status,detail,duration_ms,created_at) "
    "VALUES(?,?,?,?,?,?,strftime('%Y-%m-%dT%H:%M:%fZ','now')); ");
  bind_text(st.get(),1,test.id);bind_text(st.get(),2,test.category);bind_text(st.get(),3,profile);
  bind_text(st.get(),4,status);bind_text(st.get(),5,detail);
  sqlite3_bind_int64(st.get(),6,duration_ms);
  if(sqlite3_step(st.get())!=SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db));
}
std::string mime_for(const std::string& path) {
  if(path.ends_with(".js")) return "text/javascript; charset=utf-8";
  if(path.ends_with(".css")) return "text/css; charset=utf-8";
  return "text/html; charset=utf-8";
}
std::string read_file(const std::filesystem::path& f) {
  std::ifstream in(f,std::ios::binary);
  if(!in) throw std::runtime_error("Arquivo de interface ausente: "+f.string());
  return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
}
std::string json_escape(const std::string& text) {
  std::string out; out.reserve(text.size()+8);
  const char* hex="0123456789abcdef";
  for (unsigned char c : text) {
    switch(c) {
    case '"':out+="\\\"";break;
    case '\\':out+="\\\\";break;
    case '\n':out+="\\n";break;
    case '\r':out+="\\r";break;
    case '\t':out+="\\t";break;
    default:
      if(c<32) {out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
      else out+=static_cast<char>(c);
    }
  }
  return out;
}
std::vector<TestDefinition> parse_catalog(const std::string& filename) {
  boost::property_tree::ptree root;
  boost::property_tree::read_json(filename,root);
  if(root.get<std::string>("schema_version")!="crivo.catalog/1.0.0")
    throw std::runtime_error("Versao de schema de catalogo nao reconhecida");
  std::vector<TestDefinition> result;
  std::set<std::string> identifiers;
  const std::regex id_pattern("^[a-z][a-z0-9_.-]{2,100}$");
  for(const auto& entry:root.get_child("tests")) {
    const auto& node=entry.second;
    TestDefinition x{
      node.get<std::string>("id"),node.get<std::string>("name"),node.get<std::string>("category"),
      node.get<std::string>("subcategory"),node.get<std::string>("purpose"),node.get<std::string>("status"),
      node.get<std::string>("engine"),as_strings(node.get_child("profiles")),as_strings(node.get_child("tags"))};
    if(!std::regex_match(x.id,id_pattern)||!identifiers.insert(x.id).second)
      throw std::runtime_error("ID ausente, invalido ou duplicado: "+x.id);
    if(x.category.empty() || x.subcategory.empty() || x.profiles.empty() || x.name.empty())
      throw std::runtime_error("Atributos obrigatorios ausentes: "+x.id);
    if(x.status!="implemented" && x.status!="planned")
      throw std::runtime_error("Status desconhecido: "+x.id);
    if(x.engine!="builtin" && x.engine!="adapter")
      throw std::runtime_error("Engine desconhecido: "+x.id);
    result.push_back(std::move(x));
  }
  if(result.empty()) throw std::runtime_error("Catalogo vazio");
  return result;
}
std::string serialize_catalog(const std::vector<TestDefinition>& catalog) {
  std::string out="[";
  for(const auto& row : catalog) { if(out.size()>1) out+=','; out+=json_row(row); }
  return out+"]";
}
std::string serialize_services(const std::vector<TestDefinition>& catalog) {
  std::string out="{\"schema_version\":\"crivo.service-catalog/0.1.0\",\"services\":[";
  bool first=true;
  for(const auto& row : catalog) {
    if(!first) out+=',';
    first=false;
    out+=service_row(row);
  }
  return out+"]}";
}
std::string serialize_service(const std::vector<TestDefinition>& catalog,const std::string& id) {
  const auto found=std::find_if(catalog.begin(),catalog.end(),[&id](const auto& row) {
    return row.id==id;
  });
  if(found==catalog.end()) throw std::runtime_error("Servico desconhecido: "+id);
  return service_row(*found);
}
void initialize_db(const std::string& path) {
  const auto parent=std::filesystem::path(path).parent_path();
  if(!parent.empty()) std::filesystem::create_directories(parent);
  auto db=open_db(path);
  exec(db.get(),"PRAGMA journal_mode=WAL;");
  exec(db.get(),"PRAGMA foreign_keys=ON;");
  exec(db.get(),"CREATE TABLE IF NOT EXISTS test_catalog (id TEXT PRIMARY KEY, name TEXT NOT NULL, "
     "category TEXT NOT NULL, subcategory TEXT NOT NULL, purpose TEXT NOT NULL, "
     "status TEXT NOT NULL, engine TEXT NOT NULL, profiles TEXT NOT NULL, tags TEXT NOT NULL);");
  exec(db.get(),"CREATE TABLE IF NOT EXISTS runs (id INTEGER PRIMARY KEY AUTOINCREMENT, "
     "test_id TEXT NOT NULL, category TEXT NOT NULL, profile TEXT NOT NULL, "
     "status TEXT NOT NULL, detail TEXT NOT NULL, duration_ms INTEGER NOT NULL, "
     "created_at TEXT NOT NULL);");
  exec(db.get(),"CREATE INDEX IF NOT EXISTS idx_runs_latest ON runs(created_at DESC,id DESC);");
  exec(db.get(),"CREATE TABLE IF NOT EXISTS external_runs ("
     "run_id TEXT PRIMARY KEY, project_id TEXT NOT NULL, mode TEXT NOT NULL, "
     "source_revision TEXT NOT NULL, source_worktree TEXT NOT NULL, status TEXT NOT NULL, "
     "total INTEGER NOT NULL, passed INTEGER NOT NULL, failed INTEGER NOT NULL, skipped INTEGER NOT NULL, "
     "started_at TEXT NOT NULL, ended_at TEXT NOT NULL, duration_ms INTEGER NOT NULL, "
     "adapter TEXT NOT NULL, adapter_version TEXT NOT NULL, recorded_at TEXT NOT NULL);");
  exec(db.get(),"CREATE INDEX IF NOT EXISTS idx_external_runs_project_time "
     "ON external_runs(project_id,started_at DESC);");
  exec(db.get(),"CREATE TABLE IF NOT EXISTS evidence_index ("
     "evidence_id TEXT PRIMARY KEY, run_id TEXT NOT NULL REFERENCES external_runs(run_id), "
     "evidence_path TEXT NOT NULL, artifact_path TEXT NOT NULL, artifact_format TEXT NOT NULL, "
     "artifact_sha256 TEXT NOT NULL, evidence_state TEXT NOT NULL, recorded_at TEXT NOT NULL);");
}

void record_external_run(const std::string& path,const ExternalRunRecord& run) {
  initialize_db(path);
  auto db=open_db(path);
  exec(db.get(),"PRAGMA foreign_keys=ON;");
  exec(db.get(),"BEGIN IMMEDIATE;");
  try {
    auto st=prepare(db.get(),"INSERT INTO external_runs VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,strftime('%Y-%m-%dT%H:%M:%fZ','now')) "
      "ON CONFLICT(run_id) DO UPDATE SET project_id=excluded.project_id,mode=excluded.mode,"
      "source_revision=excluded.source_revision,source_worktree=excluded.source_worktree,status=excluded.status,"
      "total=excluded.total,passed=excluded.passed,failed=excluded.failed,skipped=excluded.skipped,"
      "started_at=excluded.started_at,ended_at=excluded.ended_at,duration_ms=excluded.duration_ms,"
      "adapter=excluded.adapter,adapter_version=excluded.adapter_version;");
    bind_text(st.get(),1,run.evidence_id); bind_text(st.get(),2,run.project_id);
    bind_text(st.get(),3,run.mode); bind_text(st.get(),4,run.source_revision);
    bind_text(st.get(),5,run.source_worktree); bind_text(st.get(),6,run.status);
    sqlite3_bind_int64(st.get(),7,run.total); sqlite3_bind_int64(st.get(),8,run.passed);
    sqlite3_bind_int64(st.get(),9,run.failed); sqlite3_bind_int64(st.get(),10,run.skipped);
    bind_text(st.get(),11,run.started_at); bind_text(st.get(),12,run.ended_at);
    sqlite3_bind_int64(st.get(),13,run.duration_ms); bind_text(st.get(),14,run.adapter);
    bind_text(st.get(),15,run.adapter_version);
    if(sqlite3_step(st.get())!=SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db.get()));

    auto ev=prepare(db.get(),"INSERT INTO evidence_index VALUES(?,?,?,?,?,?,'available',strftime('%Y-%m-%dT%H:%M:%fZ','now')) "
      "ON CONFLICT(evidence_id) DO UPDATE SET evidence_path=excluded.evidence_path,"
      "artifact_path=excluded.artifact_path,artifact_format=excluded.artifact_format,"
      "artifact_sha256=excluded.artifact_sha256,evidence_state=excluded.evidence_state;");
    bind_text(ev.get(),1,run.evidence_id); bind_text(ev.get(),2,run.evidence_id);
    bind_text(ev.get(),3,run.evidence_path); bind_text(ev.get(),4,run.junit_path);
    bind_text(ev.get(),5,"junit_xml"); bind_text(ev.get(),6,run.junit_sha256);
    if(sqlite3_step(ev.get())!=SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db.get()));
    exec(db.get(),"COMMIT;");
  } catch (...) { exec(db.get(),"ROLLBACK;"); throw; }
}

void record_external_evidence(const std::string& path,const std::string& evidence_dir) {
  const auto dir=std::filesystem::canonical(evidence_dir);
  boost::property_tree::ptree evidence,context;
  boost::property_tree::read_json((dir/"evidence.json").string(),evidence);
  boost::property_tree::read_json((dir/"pilot-context.json").string(),context);
  if(evidence.get<std::string>("schema_version")!="crivo.evidence/1.0.0" ||
     context.get<std::string>("schema_version")!="crivo.pilot-context/1.0.0")
    throw std::runtime_error("Schema de evidencia externa invalido");
  ExternalRunRecord run;
  run.evidence_id=evidence.get<std::string>("evidence_id");
  run.project_id=evidence.get<std::string>("project_id");
  if(run.project_id!=context.get<std::string>("project_id"))
    throw std::runtime_error("Projeto divergente entre evidencia e contexto");
  run.mode=context.get<std::string>("mode");
  run.source_revision=context.get<std::string>("source_revision");
  run.source_worktree=context.get<std::string>("source_worktree");
  run.total=evidence.get<long long>("summary.total");
  run.passed=evidence.get<long long>("summary.passed");
  run.failed=evidence.get<long long>("summary.failed");
  run.skipped=evidence.get<long long>("summary.skipped");
  run.status=run.failed==0?"PASS":"FAIL";
  run.started_at=evidence.get<std::string>("timing.start_utc");
  run.ended_at=evidence.get<std::string>("timing.end_utc");
  run.duration_ms=static_cast<long long>(evidence.get<double>("timing.duration_seconds")*1000.0);
  run.adapter=evidence.get<std::string>("executor.adapter");
  run.adapter_version=evidence.get<std::string>("executor.version");
  const auto artifact=evidence.get_child("artifacts").begin()->second;
  run.junit_path=(dir/artifact.get<std::string>("path")).string();
  run.junit_sha256=artifact.get<std::string>("sha256");
  const auto actual=registry::compute_sha256_hex(read_file(run.junit_path));
  if(actual!=run.junit_sha256) throw std::runtime_error("SHA-256 do artefato nao confere");
  run.evidence_path=(dir/"evidence.json").string();
  record_external_run(path,run);
}

std::string query_external_runs(const std::string& path) {
  auto db=open_db(path,true);
  auto st=prepare(db.get(),"SELECT r.run_id,r.project_id,r.mode,r.source_revision,r.source_worktree,"
    "r.status,r.total,r.passed,r.failed,r.skipped,r.started_at,r.ended_at,r.duration_ms,"
    "r.adapter,r.adapter_version,COALESCE(e.evidence_path,''),COALESCE(e.artifact_path,''),"
    "COALESCE(e.artifact_sha256,''),COALESCE(e.evidence_state,'available') "
    "FROM external_runs r LEFT JOIN evidence_index e ON e.run_id=r.run_id ORDER BY r.started_at DESC LIMIT 100;");
  std::string out="[";
  while(sqlite3_step(st.get())==SQLITE_ROW) {
    if(out.size()>1) out+=',';
    out+="{\"run_id\":\""+json_escape(str(st.get(),0))+"\",\"project_id\":\""+json_escape(str(st.get(),1))+
      "\",\"mode\":\""+json_escape(str(st.get(),2))+"\",\"source_revision\":\""+json_escape(str(st.get(),3))+
      "\",\"source_worktree\":\""+json_escape(str(st.get(),4))+"\",\"status\":\""+json_escape(str(st.get(),5))+
      "\",\"summary\":{\"total\":"+std::to_string(sqlite3_column_int64(st.get(),6))+
      ",\"passed\":"+std::to_string(sqlite3_column_int64(st.get(),7))+
      ",\"failed\":"+std::to_string(sqlite3_column_int64(st.get(),8))+
      ",\"skipped\":"+std::to_string(sqlite3_column_int64(st.get(),9))+"},\"started_at\":\""+
      json_escape(str(st.get(),10))+"\",\"ended_at\":\""+json_escape(str(st.get(),11))+
      "\",\"duration_ms\":"+std::to_string(sqlite3_column_int64(st.get(),12))+
      ",\"adapter\":\""+json_escape(str(st.get(),13))+"\",\"adapter_version\":\""+json_escape(str(st.get(),14))+
      "\",\"evidence_path\":\""+json_escape(str(st.get(),15))+"\",\"artifact_path\":\""+json_escape(str(st.get(),16))+
      "\",\"artifact_sha256\":\""+json_escape(str(st.get(),17))+"\",\"evidence_state\":\""+json_escape(str(st.get(),18))+"\"}";
  }
  return out+"]";
}
std::string query_builtin_runs(const std::string& path) {
  auto db=open_db(path,true);
  auto st=prepare(db.get(),"SELECT id, test_id, category, profile, status, detail, duration_ms, created_at FROM runs ORDER BY created_at DESC, id DESC LIMIT 100;");
  std::string out="[";
  while(sqlite3_step(st.get())==SQLITE_ROW) {
    if(out.size()>1) out+=',';
    out+="{\"id\":"+std::to_string(sqlite3_column_int64(st.get(),0))+
      ",\"test_id\":\""+json_escape(str(st.get(),1))+
      "\",\"category\":\""+json_escape(str(st.get(),2))+
      "\",\"profile\":\""+json_escape(str(st.get(),3))+
      "\",\"status\":\""+json_escape(str(st.get(),4))+
      "\",\"detail\":\""+json_escape(str(st.get(),5))+
      "\",\"duration_ms\":"+std::to_string(sqlite3_column_int64(st.get(),6))+
      ",\"created_at\":\""+json_escape(str(st.get(),7))+"\"}";
  }
  return out+"]";
}
void register_catalog(const std::string& path,const std::vector<TestDefinition>& catalog) {
  auto db=open_db(path);
  exec(db.get(),"BEGIN IMMEDIATE;");
  try {
    exec(db.get(),"DELETE FROM test_catalog;");
    for (const auto& d:catalog) {
      auto st=prepare(db.get(),"INSERT INTO test_catalog VALUES(?,?,?,?,?,?,?,?,?);");
      bind_text(st.get(),1,d.id);bind_text(st.get(),2,d.name);bind_text(st.get(),3,d.category);
      bind_text(st.get(),4,d.subcategory);bind_text(st.get(),5,d.purpose);bind_text(st.get(),6,d.status);
      bind_text(st.get(),7,d.engine);bind_text(st.get(),8,json_array(d.profiles));bind_text(st.get(),9,json_array(d.tags));
      if(sqlite3_step(st.get())!=SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db.get()));
    }
    exec(db.get(),"COMMIT;");
  } catch (...) { exec(db.get(),"ROLLBACK;"); throw; }
}
int run_profile(const std::string& path,const std::string& file,const std::string& profile) {
  const auto all=parse_catalog(file);register_catalog(path,all);
  auto db=open_db(path);
  int selected=0, failed=0, blocked=0;
  for(const auto& d:all) {
    if(std::find(d.profiles.begin(),d.profiles.end(),profile)==d.profiles.end()) continue;
    ++selected;
    const auto begin=std::chrono::steady_clock::now();
    std::string status,detail;
    if(d.status!="implemented" || d.engine!="builtin") {
      status="BLOCKED";detail="Teste declarado mas nao implementado"; ++blocked;
    } else {
      try {
        const auto [ok,reason]=execute_builtin(d,db.get(),file);
        status=ok?"PASS":"FAIL";detail=reason;
        if(!ok) ++failed;
      } catch (const std::exception& e) {status="FAIL";detail=e.what();++failed;}
    }
    const auto milliseconds=std::chrono::duration_cast<std::chrono::milliseconds>(
       std::chrono::steady_clock::now()-begin).count();
    save_run(db.get(),d,profile,status,detail,milliseconds);
    std::cout<<status<<"\t"<<d.id<<"\t"<<detail<<"\n";
  }
  if(!selected) {std::cerr<<"Nenhum teste selecionado para o perfil '"<<profile<<"'\n";return 2;}
  std::cout<<"Selecionados: "<<selected<<" | falhas: "<<failed<<" | bloqueados: "<<blocked<<"\n";
  return (failed||blocked)?1:0;
}
std::string query_json(const std::string& path,const std::string& name,bool implemented_only) {
  auto db=open_db(path,true);
  if(name=="overview") {
    const auto total=prepare(db.get(),"SELECT COUNT(*), SUM(status='implemented') FROM test_catalog;");
    sqlite3_step(total.get());
    const auto latest=prepare(db.get(),"SELECT COUNT(*),SUM(status='PASS'),SUM(status='FAIL'),SUM(status='BLOCKED') "
        "FROM runs WHERE id IN (SELECT MAX(id) FROM runs GROUP BY test_id);");
    sqlite3_step(latest.get());
    return "{\"tests\":"+std::to_string(sqlite3_column_int(total.get(),0))+
      ",\"implemented\":"+std::to_string(sqlite3_column_int(total.get(),1))+
      ",\"executed\":"+std::to_string(sqlite3_column_int(latest.get(),0))+
      ",\"passed\":"+std::to_string(sqlite3_column_int(latest.get(),1))+
      ",\"failed\":"+std::to_string(sqlite3_column_int(latest.get(),2))+
      ",\"blocked\":"+std::to_string(sqlite3_column_int(latest.get(),3))+"}";
  }
  if(name=="categories") {
    const auto st=prepare(db.get(),"SELECT category,COUNT(*) AS n,SUM(status='implemented') FROM test_catalog GROUP BY category ORDER BY category;");
    std::string out="[";
    while(sqlite3_step(st.get())==SQLITE_ROW) {
      if(out.size()>1) out+=',';
      out+="{\"category\":\""+json_escape(str(st.get(),0))+"\",\"tests\":"+
        std::to_string(sqlite3_column_int(st.get(),1))+",\"implemented\":"+
        std::to_string(sqlite3_column_int(st.get(),2))+"}";
    }
    return out+"]";
  }
  if(name=="tests") {
    const auto st=prepare(db.get(),"SELECT id,name,category,subcategory,purpose,status,engine,profiles,tags FROM test_catalog ORDER BY category,id;");
    std::string out="[";
    while(sqlite3_step(st.get())==SQLITE_ROW) {
      if(out.size()>1) out+=',';
      out+="{\"id\":\""+json_escape(str(st.get(),0))+"\",\"name\":\""+json_escape(str(st.get(),1))+
        "\",\"category\":\""+json_escape(str(st.get(),2))+"\",\"subcategory\":\""+json_escape(str(st.get(),3))+
        "\",\"purpose\":\""+json_escape(str(st.get(),4))+"\",\"status\":\""+json_escape(str(st.get(),5))+
        "\",\"engine\":\""+json_escape(str(st.get(),6))+"\",\"profiles\":"+str(st.get(),7)+
        ",\"tags\":"+str(st.get(),8)+"}";
    }
    return out+"]";
  }
  if(name=="services") {
    const auto st=prepare(db.get(),implemented_only?
      "SELECT id,name,category,subcategory,purpose,status,engine,profiles,tags FROM test_catalog WHERE status='implemented' ORDER BY category,id;":
      "SELECT id,name,category,subcategory,purpose,status,engine,profiles,tags FROM test_catalog ORDER BY category,id;");
    std::vector<TestDefinition> catalog;
    while(sqlite3_step(st.get())==SQLITE_ROW) {
      TestDefinition row{str(st.get(),0),str(st.get(),1),str(st.get(),2),str(st.get(),3),
        str(st.get(),4),str(st.get(),5),str(st.get(),6),{}, {}};
      boost::property_tree::ptree profiles;
      std::istringstream profile_json(str(st.get(),7));
      boost::property_tree::read_json(profile_json,profiles);
      row.profiles=as_strings(profiles);
      catalog.push_back(std::move(row));
    }
    return serialize_services(catalog);
  }
  if(name=="registry/overview") {
    auto count_table = [&](const std::string& tbl) -> int {
      try {
        const auto st = prepare(db.get(), "SELECT COUNT(*) FROM " + tbl + ";");
        if (sqlite3_step(st.get()) == SQLITE_ROW) return sqlite3_column_int(st.get(), 0);
      } catch (...) {}
      return 0;
    };
    int refs = count_table("reference_versions");
    int techs = count_table("technique_versions");
    int specs = count_table("test_spec_versions");
    int impls = count_table("test_implementation_versions");
    int profs = count_table("profile_versions");
    int evts = count_table("lifecycle_events");

    return "{\"schema_version\":\"crivo.registry-overview/1.0.0\","
           "\"references\":" + std::to_string(refs) +
           ",\"techniques\":" + std::to_string(techs) +
           ",\"specifications\":" + std::to_string(specs) +
           ",\"implementations\":" + std::to_string(impls) +
           ",\"profiles\":" + std::to_string(profs) +
           ",\"events\":" + std::to_string(evts) + "}";
  }
  if(name=="registry/references") {
    try {
      const auto st = prepare(db.get(), "SELECT reference_id, edition, issuer, kind, title, COALESCE(status_at_registration,''), url, distribution, license_review, catalog_level, COALESCE(notes,'') FROM reference_versions ORDER BY reference_id, edition;");
      std::string out = "[";
      while (sqlite3_step(st.get()) == SQLITE_ROW) {
        if (out.size() > 1) out += ',';
        out += "{\"id\":\"" + json_escape(str(st.get(), 0)) +
               "\",\"edition\":\"" + json_escape(str(st.get(), 1)) +
               "\",\"issuer\":\"" + json_escape(str(st.get(), 2)) +
               "\",\"kind\":\"" + json_escape(str(st.get(), 3)) +
               "\",\"title\":\"" + json_escape(str(st.get(), 4)) +
               "\",\"status_at_registration\":\"" + json_escape(str(st.get(), 5)) +
               "\",\"url\":\"" + json_escape(str(st.get(), 6)) +
               "\",\"distribution\":\"" + json_escape(str(st.get(), 7)) +
               "\",\"license_review\":\"" + json_escape(str(st.get(), 8)) +
               "\",\"catalog_level\":\"" + json_escape(str(st.get(), 9)) +
               "\",\"notes\":\"" + json_escape(str(st.get(), 10)) + "\"}";
      }
      return out + "]";
    } catch (...) { return "[]"; }
  }
  if(name=="registry/techniques") {
    try {
      const auto st = prepare(db.get(), "SELECT technique_id, version, family, name, procedure_summary, oracle_class, distribution FROM technique_versions ORDER BY technique_id, version;");
      std::string out = "[";
      while (sqlite3_step(st.get()) == SQLITE_ROW) {
        if (out.size() > 1) out += ',';
        out += "{\"id\":\"" + json_escape(str(st.get(), 0)) +
               "\",\"version\":\"" + json_escape(str(st.get(), 1)) +
               "\",\"family\":\"" + json_escape(str(st.get(), 2)) +
               "\",\"name\":\"" + json_escape(str(st.get(), 3)) +
               "\",\"procedure_summary\":\"" + json_escape(str(st.get(), 4)) +
               "\",\"oracle_class\":\"" + json_escape(str(st.get(), 5)) +
               "\",\"distribution\":\"" + json_escape(str(st.get(), 6)) + "\"}";
      }
      return out + "]";
    } catch (...) { return "[]"; }
  }
  if(name=="registry/specifications") {
    try {
      const auto st = prepare(db.get(), "SELECT spec_id, version, COALESCE(title,''), source, category, subcategory, level, purpose, oracle_type, oracle_id, qualification FROM test_spec_versions ORDER BY spec_id, version;");
      std::string out = "[";
      while (sqlite3_step(st.get()) == SQLITE_ROW) {
        if (out.size() > 1) out += ',';
        out += "{\"id\":\"" + json_escape(str(st.get(), 0)) +
               "\",\"version\":\"" + json_escape(str(st.get(), 1)) +
               "\",\"title\":\"" + json_escape(str(st.get(), 2)) +
               "\",\"source\":\"" + json_escape(str(st.get(), 3)) +
               "\",\"category\":\"" + json_escape(str(st.get(), 4)) +
               "\",\"subcategory\":\"" + json_escape(str(st.get(), 5)) +
               "\",\"level\":\"" + json_escape(str(st.get(), 6)) +
               "\",\"purpose\":\"" + json_escape(str(st.get(), 7)) +
               "\",\"oracle_type\":\"" + json_escape(str(st.get(), 8)) +
               "\",\"oracle_id\":\"" + json_escape(str(st.get(), 9)) +
               "\",\"qualification\":\"" + json_escape(str(st.get(), 10)) + "\"}";
      }
      return out + "]";
    } catch (...) { return "[]"; }
  }
  if(name=="registry/events") {
    try {
      const auto st = prepare(db.get(), "SELECT event_id, entity_type, entity_ref, event_type, occurred_at, recorded_at, cause, authority_ref FROM lifecycle_events ORDER BY recorded_at DESC LIMIT 50;");
      std::string out = "[";
      while (sqlite3_step(st.get()) == SQLITE_ROW) {
        if (out.size() > 1) out += ',';
        out += "{\"event_id\":\"" + json_escape(str(st.get(), 0)) +
               "\",\"entity_type\":\"" + json_escape(str(st.get(), 1)) +
               "\",\"entity_ref\":\"" + json_escape(str(st.get(), 2)) +
               "\",\"event_type\":\"" + json_escape(str(st.get(), 3)) +
               "\",\"occurred_at\":\"" + json_escape(str(st.get(), 4)) +
               "\",\"recorded_at\":\"" + json_escape(str(st.get(), 5)) +
               "\",\"cause\":\"" + json_escape(str(st.get(), 6)) +
               "\",\"authority_ref\":\"" + json_escape(str(st.get(), 7)) + "\"}";
      }
      return out + "]";
    } catch (...) { return "[]"; }
  }
  if(name=="runs") {
    const auto st=prepare(db.get(),"SELECT id,test_id,category,profile,status,detail,duration_ms,created_at FROM runs ORDER BY id DESC LIMIT 100;");
    std::string out="[";
    while(sqlite3_step(st.get())==SQLITE_ROW) {
      if(out.size()>1) out+=',';
      out+="{\"id\":"+std::to_string(sqlite3_column_int64(st.get(),0))+
        ",\"test_id\":\""+json_escape(str(st.get(),1))+"\",\"category\":\""+json_escape(str(st.get(),2))+
        "\",\"profile\":\""+json_escape(str(st.get(),3))+"\",\"status\":\""+json_escape(str(st.get(),4))+
        "\",\"detail\":\""+json_escape(str(st.get(),5))+"\",\"duration_ms\":"+
        std::to_string(sqlite3_column_int64(st.get(),6))+
        ",\"created_at\":\""+json_escape(str(st.get(),7))+"\"}";
    }
    return out+"]";
  }
  throw std::runtime_error("Consulta nao suportada: "+name);
}
std::string query_service_json(const std::string& path,const std::string& id,bool implemented_only) {
  if(!std::regex_match(id,std::regex("^[a-z][a-z0-9_.-]{2,100}$")))
    throw std::runtime_error("ID de servico invalido");
  auto db=open_db(path,true);
  auto st=prepare(db.get(),implemented_only?
    "SELECT id,name,category,subcategory,purpose,status,engine,profiles,tags FROM test_catalog WHERE id=? AND status='implemented';":
    "SELECT id,name,category,subcategory,purpose,status,engine,profiles,tags FROM test_catalog WHERE id=?;");
  bind_text(st.get(),1,id);
  if(sqlite3_step(st.get())!=SQLITE_ROW) throw std::runtime_error("Servico desconhecido: "+id);
  TestDefinition row{str(st.get(),0),str(st.get(),1),str(st.get(),2),str(st.get(),3),
    str(st.get(),4),str(st.get(),5),str(st.get(),6),{}, {}};
  boost::property_tree::ptree profiles;
  std::istringstream profile_json(str(st.get(),7));
  boost::property_tree::read_json(profile_json,profiles);
  row.profiles=as_strings(profiles);
  return service_row(row);
}
void serve(const std::string& db,const std::string& web_directory,
           const std::string& bind_address,unsigned short port) {
  namespace asio=boost::asio;
  namespace beast=boost::beast;
  namespace http=beast::http;
  using tcp=asio::ip::tcp;
  asio::io_context context(1);
  beast::error_code address_error;
  const auto address=asio::ip::make_address(bind_address,address_error);
  if(address_error)
    throw std::runtime_error("Endereco de bind invalido: "+bind_address);
  tcp::acceptor acceptor(context,{address,port});
  const bool public_mode=!address.is_loopback();
  std::cout<<"CRIVO SisTer Web http://"<<bind_address<<":"<<port
           <<" (somente leitura)\n";
  for (;;) {
    tcp::socket socket(context);
    acceptor.accept(socket);
    try {
      beast::tcp_stream stream(std::move(socket));
      stream.expires_after(std::chrono::seconds(5));
      beast::flat_buffer buffer;
      http::request_parser<http::string_body> parser;
      parser.body_limit(1024*1024);
      http::read(stream,buffer,parser);
      const auto req=parser.get();
      const auto target=std::string(req.target());
      const auto qpos=target.find('?');
      const auto path=(qpos!=std::string::npos)?target.substr(0,qpos):target;
      http::response<http::string_body> resp{http::status::ok,req.version()};
      resp.set(http::field::server,"CRIVO/0.1.0");
      resp.set("Content-Security-Policy","default-src 'self'; script-src 'self'; style-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'");
      resp.set("X-Content-Type-Options","nosniff");
      resp.set(http::field::cache_control,"no-store");
      resp.keep_alive(false);
      if(req.method()!=http::verb::get) {
        resp.result(http::status::method_not_allowed);resp.body()="Metodo nao permitido";
      } else if(path=="/api/v1/services") {
        resp.set(http::field::content_type,"application/json; charset=utf-8");
        resp.body()=query_json(db,"services",public_mode);
      } else if(path=="/api/v1/external-runs" || path=="/api/v1/runs") {
        resp.set(http::field::content_type,"application/json; charset=utf-8");
        try { resp.body()=query_external_runs(db); }
        catch(...) { resp.body()="[]"; }
      } else if(path=="/api/v1/runs/builtin" || path=="/api/v1/runs/local") {
        resp.set(http::field::content_type,"application/json; charset=utf-8");
        try { resp.body()=query_builtin_runs(db); }
        catch(...) { resp.body()="[]"; }
      } else if(path=="/api/v1/memory/experiences") {
        resp.set(http::field::content_type,"application/json; charset=utf-8");
        try { resp.body()=crivo::memory::serialize_experiences_json(crivo::memory::query_experiences(db)); }
        catch(...) { resp.body()="[]"; }
      } else if(path=="/api/v1/dev-context" || path=="/api/v1/context") {
        resp.set(http::field::content_type,"application/json; charset=utf-8");
        try {
          auto ctx = crivo::context::build_dev_context(db, ".");
          resp.body()=crivo::context::serialize_dev_context_json(ctx);
        } catch(...) { resp.body()="{}"; }
      } else if(path=="/api/v1/overview" || path=="/api/v1/categories" || path=="/api/v1/tests") {
        const auto name=path.substr(std::string("/api/v1/").size());
        resp.set(http::field::content_type,"application/json; charset=utf-8");
        resp.body()=query_json(db,name);
      } else if(path.starts_with("/api/v1/registry/")) {
        const auto name=path.substr(std::string("/api/v1/").size());
        resp.set(http::field::content_type,"application/json; charset=utf-8");
        try { resp.body()=query_json(db,name); }
        catch(...) { resp.result(http::status::not_found); resp.body()="{\"error\":\"REGISTRY_QUERY_NOT_FOUND\"}"; }
      } else if(path.starts_with("/api/v1/services/")) {
        const auto id=path.substr(std::string("/api/v1/services/").size());
        resp.set(http::field::content_type,"application/json; charset=utf-8");
        try { resp.body()=query_service_json(db,id,public_mode); }
        catch(const std::exception&) {
          resp.result(http::status::not_found);
          resp.body()="{\"error\":\"SERVICE_NOT_FOUND\"}";
        }
      } else if(path=="/" || path=="/index.html" || path=="/app.js" || path=="/styles.css") {
        const std::string file=(path=="/" || path=="/index.html")?"index.html":path.substr(1);
        resp.set(http::field::content_type,mime_for(file));
        resp.body()=read_file(std::filesystem::path(web_directory)/file);
      } else {
        resp.result(http::status::not_found);resp.body()="Recurso nao encontrado";
      }
      resp.prepare_payload();http::write(stream,resp);
      beast::error_code ec;
      stream.socket().shutdown(tcp::socket::shutdown_both,ec);
    } catch(const std::exception& e) {std::cerr<<"HTTP: "<<e.what()<<"\n";}
  }
}
}
