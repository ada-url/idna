#include "ada/idna/utils.h"

namespace ada::idna::utils {

template <typename T, typename U>
bool begins_with(T view, U prefix) {
  if (view.size() < std::char_traits<U>::length(&prefix)) {
    return false;
  }
  return view.substr(0, std::char_traits<U>::length(&prefix)) == prefix;
}

}  // namespace ada::idna::utils