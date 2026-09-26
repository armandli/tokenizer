#include <corpus_input.h>

#include <arrow_io.h>

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace tokenizer {

s::vector<InputFile> expand_input_paths(const s::vector<s::string>& input_paths){
  s::vector<InputFile> files;
  for (const auto& input_path : input_paths){
    const fs::path path(input_path);
    if (not fs::is_directory(path)){
      files.push_back({input_path, is_arrow_path(input_path) ? InputKind::ArrowIpc
                                                             : InputKind::Raw});
      continue;
    }

    s::vector<fs::path> dir_files;
    bool has_arrow = false;
    for (const auto& entry : fs::recursive_directory_iterator(path)){
      if (not entry.is_regular_file()) continue;
      dir_files.push_back(entry.path());
      has_arrow = has_arrow or is_arrow_path(entry.path().string());
    }
    s::sort(dir_files.begin(), dir_files.end());

    for (const auto& file : dir_files){
      const s::string name = file.string();
      if (has_arrow){
        if (is_arrow_path(name)) files.push_back({name, InputKind::ArrowIpc});
      } else {
        files.push_back({name, InputKind::Raw});
      }
    }
  }
  return files;
}

} // tokenizer
