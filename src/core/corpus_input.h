#ifndef CORPUS_INPUT_H
#define CORPUS_INPUT_H

#include <string>
#include <vector>

namespace s = std;

namespace tokenizer {

// How a training input should be decoded: Raw is the whole file's bytes,
// ArrowIpc is one text column of an Arrow IPC file.
enum struct InputKind : int {
  Raw,
  ArrowIpc,
};

struct InputFile {
  s::string path;
  InputKind kind;
};

// Expands each input path into a flat list of classified files: a file is used
// as-is and classified by extension, a directory is walked recursively (in
// sorted order, for reproducible tables).
//
// A directory whose walk turns up at least one `.arrow` file is treated as an
// Arrow dataset directory and contributes only those files -- a HuggingFace
// split also carries `state.json` / `dataset_info.json` sidecars, and training
// on those would fold dataset metadata into the merge table. A directory with
// no `.arrow` file in it contributes every regular file, as raw bytes.
s::vector<InputFile> expand_input_paths(const s::vector<s::string>& input_paths);

} // tokenizer

#endif//CORPUS_INPUT_H
