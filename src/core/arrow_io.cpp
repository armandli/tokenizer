#include <arrow_io.h>

#include <arrow/array/array_binary.h>
#include <arrow/io/file.h>
#include <arrow/ipc/reader.h>
#include <arrow/record_batch.h>
#include <arrow/result.h>
#include <arrow/status.h>
#include <arrow/type.h>

#include <cstdint>
#include <fstream>
#include <memory>

// Reader for Arrow IPC files, the on-disk form of a HuggingFace `datasets`
// split (`data-00000-of-000NN.arrow` beside its JSON sidecars).
//
// The `.arrow` extension covers two distinct IPC encodings: the random-access
// *file* format, which opens with the magic "ARROW1", and the *stream* format,
// which opens straight into a continuation marker. `datasets` writes the stream
// form, but both turn up in the wild, so the encoding is sniffed from the first
// bytes rather than assumed.

namespace tokenizer {

namespace {

namespace aio = arrow::io;
namespace aipc = arrow::ipc;

constexpr s::string_view kArrowExtension = ".arrow";
constexpr s::string_view kFileFormatMagic = "ARROW1";

[[noreturn]] void fail(const s::string& path, const s::string& what){
  throw arrow_error("arrow file \"" + path + "\": " + what);
}

// Arrow reports every failure as a Status, and none of them are recoverable
// here, so a bad one becomes an arrow_error naming the file.
template <typename T>
T value_or_fail(arrow::Result<T> res, const s::string& path, const s::string& what){
  if (not res.ok()) fail(path, what + ": " + res.status().ToString());
  return res.MoveValueUnsafe();
}

bool has_file_format_magic(const s::string& path){
  s::ifstream in(path, s::ios::binary);
  if (not in) fail(path, "cannot open for reading");
  char magic[kFileFormatMagic.size()] = {};
  in.read(magic, (s::streamsize)sizeof(magic));
  // A file shorter than the magic cannot be the file format; let the IPC reader
  // produce the real diagnostic rather than guessing at one here.
  if (in.gcount() != (s::streamsize)sizeof(magic)) return false;
  return s::string_view(magic, sizeof(magic)) == kFileFormatMagic;
}

// Every byte-string array type exposes the same GetView/IsNull pair, so the
// concrete type only picks the offset width and inline-vs-indirect storage.
template <typename ArrayT>
void append_values(const arrow::Array& col, TextRows& rows){
  const auto& values = static_cast<const ArrayT&>(col);
  for (int64_t i = 0; i < values.length(); ++i){
    if (values.IsNull(i)) continue;
    const s::string_view value = values.GetView(i);
    rows.emplace_back(value.begin(), value.end());
  }
}

void append_column(const arrow::Array& col, const s::string& path,
                   const s::string& name, TextRows& rows){
  switch (col.type_id()){
    case arrow::Type::STRING:
      append_values<arrow::StringArray>(col, rows);

    break; case arrow::Type::LARGE_STRING:
      append_values<arrow::LargeStringArray>(col, rows);

    break; case arrow::Type::STRING_VIEW:
      append_values<arrow::StringViewArray>(col, rows);

    break; case arrow::Type::BINARY:
      append_values<arrow::BinaryArray>(col, rows);

    break; case arrow::Type::LARGE_BINARY:
      append_values<arrow::LargeBinaryArray>(col, rows);

    break; case arrow::Type::BINARY_VIEW:
      append_values<arrow::BinaryViewArray>(col, rows);

    break; default:
      fail(path, "column \"" + name + "\" has type " +
                     col.type()->ToString() + ", which does not hold byte strings");
  }
}

// The error lists the schema's own column names: a typo in --column is the
// likeliest way to get here.
int column_index(const arrow::Schema& schema, const s::string& path,
                 const s::string& name){
  const int index = schema.GetFieldIndex(name);
  if (index >= 0) return index;
  s::string available;
  for (const auto& field : schema.fields()){
    if (not available.empty()) available += ", ";
    available += '"' + field->name() + '"';
  }
  if (available.empty()) available = "(none)";
  fail(path, "no column named \"" + name + "\"; available columns: " + available);
}

// Memory-mapped so a multi-hundred-MB shard is not also copied onto the heap.
s::shared_ptr<aio::MemoryMappedFile> open_mapped(const s::string& path){
  return value_or_fail(aio::MemoryMappedFile::Open(path, aio::FileMode::READ),
                       path, "cannot open");
}

TextRows read_file_format(const s::string& path, const s::string& column){
  auto file = open_mapped(path);
  auto reader = value_or_fail(aipc::RecordBatchFileReader::Open(file), path,
                              "not a readable Arrow IPC file");
  const int index = column_index(*reader->schema(), path, column);

  TextRows rows;
  for (int i = 0; i < reader->num_record_batches(); ++i){
    auto batch = value_or_fail(reader->ReadRecordBatch(i), path,
                               "cannot read record batch");
    append_column(*batch->column(index), path, column, rows);
  }
  return rows;
}

TextRows read_stream_format(const s::string& path, const s::string& column){
  auto file = open_mapped(path);
  auto reader = value_or_fail(aipc::RecordBatchStreamReader::Open(file), path,
                              "not a readable Arrow IPC stream");
  const int index = column_index(*reader->schema(), path, column);

  TextRows rows;
  for (;;){
    s::shared_ptr<arrow::RecordBatch> batch;
    const arrow::Status status = reader->ReadNext(&batch);
    if (not status.ok()) fail(path, "cannot read batch: " + status.ToString());
    if (batch == nullptr) break; // a null batch is how ReadNext signals the end
    append_column(*batch->column(index), path, column, rows);
  }
  return rows;
}

} // namespace

bool is_arrow_path(s::string_view path){
  return path.size() > kArrowExtension.size() and
         path.substr(path.size() - kArrowExtension.size()) == kArrowExtension;
}

TextRows read_arrow_column(const s::string& path, const s::string& column){
  if (has_file_format_magic(path)) return read_file_format(path, column);
  return read_stream_format(path, column);
}

} // tokenizer
