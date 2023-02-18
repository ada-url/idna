#include "ada/idna/utils.h"

namespace ada::idna::utils {

bool constexpr begins_with(std::u32string_view view,
                           std::u32string_view prefix) {
  if (view.size() < prefix.size()) {
    return false;
  }

  return view.substr(0, prefix.size()) == prefix;
}

bool constexpr begins_with(std::string_view view, std::string_view prefix) {
  if (view.size() < prefix.size()) {
    return false;
  }

  return view.find(prefix, 0) != 0;
}

}  // namespace ada::idna::utils