// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <cstdio>

#include "csharp_options.h"

// The normal Bazel build uses C++17; CMake also supports C++14 consumers.
#if defined(_MSVC_LANG)
#if _MSVC_LANG != 201402L
#error "Compile this regression with C++14"
#endif
#elif __cplusplus != 201402L
#error "Compile this regression with C++14"
#endif

int main() {
  google::protobuf::compiler::csharp::Options options;
  if (options.source_links.has_value() ||
      options.source_links_text_prefix.has_value()) {
    return 1;
  }

  options.source_links = "";
  options.source_links_text_prefix = "";
  if (!options.source_links.has_value() || !options.source_links->empty() ||
      !options.source_links_text_prefix.has_value() ||
      !options.source_links_text_prefix->empty()) {
    return 2;
  }

  options.source_links = "vscode://source?path={path}&line={line}";
  options.source_links_text_prefix = "Assets/";
  if (*options.source_links != "vscode://source?path={path}&line={line}" ||
      options.source_links_text_prefix.value_or("fallback") != "Assets/") {
    return 3;
  }

  options.source_links.reset();
  options.source_links_text_prefix.reset();
  if (options.source_links.has_value() ||
      options.source_links_text_prefix.has_value() ||
      options.source_links_text_prefix.value_or("fallback") != "fallback") {
    return 4;
  }
  std::puts("C++14 Options: absent, empty, populated and reset states passed");
  return 0;
}
