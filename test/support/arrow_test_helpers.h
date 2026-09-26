#ifndef ARROW_TEST_HELPERS_H
#define ARROW_TEST_HELPERS_H

#include <arrow/array/builder_binary.h>
#include <arrow/array/builder_primitive.h>
#include <arrow/io/file.h>
#include <arrow/ipc/writer.h>
#include <arrow/record_batch.h>
#include <arrow/type.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Writes the .arrow fixtures the tests then read back. Kept header-only so both
// the unit and integration targets can use it; they link Arrow only for this --
// production code merely reads.
namespace arrow_test {

// A row of the text column; std::nullopt is a null entry.
using Row = std::optional<std::string>;

enum struct Encoding : int {
  Stream, // IPC stream format, what HuggingFace `datasets` writes
  File,   // IPC random-access file format, opens with "ARROW1"
};

inline void check(const arrow::Status& status, const char* what) {
  if (status.ok()) return;
  throw std::runtime_error(std::string(what) + ": " + status.ToString());
}

template <typename T>
T unwrap(arrow::Result<T> result, const char* what) {
  check(result.status(), what);
  return result.MoveValueUnsafe();
}

// One record batch per element of `batches`. The second column ("id", int32)
// is there so a reader that ignores the requested column name cannot pass by
// accident.
inline void write_arrow_fixture(const std::string& path, Encoding encoding,
                                const std::vector<std::vector<Row>>& batches,
                                const std::string& text_column = "text") {
  const auto schema = arrow::schema({arrow::field(text_column, arrow::utf8()),
                                     arrow::field("id", arrow::int32())});

  auto sink = unwrap(arrow::io::FileOutputStream::Open(path), "open fixture");
  auto writer =
      encoding == Encoding::File
          ? unwrap(arrow::ipc::MakeFileWriter(sink, schema), "file writer")
          : unwrap(arrow::ipc::MakeStreamWriter(sink, schema), "stream writer");

  std::int32_t next_id = 0;
  for (const auto& batch : batches) {
    arrow::StringBuilder text;
    arrow::Int32Builder id;
    for (const auto& row : batch) {
      check(row.has_value() ? text.Append(*row) : text.AppendNull(), "text");
      check(id.Append(next_id++), "append id");
    }
    std::shared_ptr<arrow::Array> text_array;
    std::shared_ptr<arrow::Array> id_array;
    check(text.Finish(&text_array), "finish text");
    check(id.Finish(&id_array), "finish id");
    check(writer->WriteRecordBatch(*arrow::RecordBatch::Make(
              schema, (std::int64_t)batch.size(), {text_array, id_array})),
          "write record batch");
  }

  check(writer->Close(), "close writer");
  check(sink->Close(), "close fixture");
}

} // arrow_test

#endif//ARROW_TEST_HELPERS_H
