#ifndef ARROW_IO_H
#define ARROW_IO_H

#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>

namespace s = std;

namespace tokenizer {

// One raw byte buffer per row of a text column, in file order.
using TextRows = s::vector<s::vector<char>>;

struct arrow_error : s::runtime_error {
  arrow_error(const s::string& msg): s::runtime_error(msg) {}
};

// Both IPC encodings use the `.arrow` extension and are told apart by their
// content, not their name.
bool is_arrow_path(s::string_view path);

// Null rows are skipped. Throws arrow_error on an I/O failure, a missing
// column, or a column whose type does not hold byte strings.
TextRows read_arrow_column(const s::string& path, const s::string& column);

} // tokenizer

#endif//ARROW_IO_H
