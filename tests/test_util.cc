#include "test_util.h"

#include <filesystem>
#include <string>
#include <vector>

#include "absl/time/time.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"

namespace witnesskvs::test {

absl::Status Cleanup(std::vector<std::string> filenames) {
  bool success = true;
  for (auto& filename : filenames) {
    success =
        success && std::filesystem::remove(std::filesystem::path(filename));
  }
  if (success) {
    return absl::OkStatus();
  }
  return absl::UnknownError(
      absl::StrCat("Could not delete files: ", absl::StrJoin(filenames, ",")));
}

std::string GetTempPrefix(std::string base_prefix) {
  absl::Time now = absl::Now();
  base_prefix.append(absl::StrCat(absl::ToUnixMicros(now)));
  base_prefix.append("_test");
  return base_prefix;
}


}  // namespace witnesskvs::test
