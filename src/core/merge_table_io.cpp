#include <bpe_builder.h>

#include <simdjson.h>

#include <fstream>
#include <stdexcept>

// JSON persistence for a MergeTable. Schema (written on one line, trailing \n):
//
//   {"format":"tokenizer-bpe-merges","version":1,
//    "merges":[[left,right,id],[left,right,id],...]}
//
// `merges` is the table in learned order (index i -> id 256 + i); each entry is
// the unpacked adjacent pair plus its new id. Writing is a hand-rolled string
// (all fields are small non-negative integers -- no escaping); reading goes
// through simdjson so malformed input is rejected rather than half-parsed.

namespace tokenizer {

namespace {

constexpr const char* kFormat = "tokenizer-bpe-merges";
constexpr int kVersion = 1;

[[noreturn]] void fail(const s::string& what, const s::string& path){
  throw s::runtime_error("merge table \"" + path + "\": " + what);
}

} // namespace

void save_merge_table(const s::string& path, const MergeTable& merges){
  s::ofstream out(path, s::ios::binary | s::ios::trunc);
  if (not out) fail("cannot open for writing", path);

  out << "{\"format\":\"" << kFormat << "\",\"version\":" << kVersion
      << ",\"merges\":[";
  for (size_t i = 0; i < merges.size(); ++i){
    auto [left, right] = unpack_pair(merges[i].first);
    if (i) out << ',';
    out << '[' << left << ',' << right << ',' << merges[i].second << ']';
  }
  out << "]}\n";

  out.flush();
  if (not out) fail("write failed", path);
}

MergeTable load_merge_table(const s::string& path){
  simdjson::dom::parser parser;
  simdjson::dom::element doc;
  if (auto err = parser.load(path).get(doc); err)
    fail(simdjson::error_message(err), path);

  s::string_view format;
  if (doc["format"].get(format) || format != kFormat)
    fail(s::string("not a ") + kFormat + " file", path);

  simdjson::dom::array entries;
  if (doc["merges"].get(entries))
    fail("missing \"merges\" array", path);

  MergeTable merges;
  for (simdjson::dom::element entry : entries){
    simdjson::dom::array triple;
    if (entry.get(triple)) fail("merge entry is not an array", path);

    uint64_t v[3];
    size_t n = 0;
    for (simdjson::dom::element field : triple){
      if (n == 3) fail("merge entry must be [left, right, id]", path);
      if (field.get(v[n])) fail("merge entry has a non-integer field", path);
      ++n;
    }
    if (n != 3) fail("merge entry must be [left, right, id]", path);

    merges.emplace_back(pack_pair((CP)v[0], (CP)v[1]), (CP)v[2]);
  }
  return merges;
}

} // tokenizer
