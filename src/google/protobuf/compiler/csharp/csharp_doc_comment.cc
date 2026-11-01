// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
#include "google/protobuf/compiler/csharp/csharp_doc_comment.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "google/protobuf/compiler/csharp/csharp_options.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/printer.h"

namespace google {
namespace protobuf {
namespace compiler {
namespace csharp {

// Functions to create C# XML documentation comments.
// Documentation generated from names rather than source comments is inlined in
// the relevant generators.

void WriteDocCommentBodyImpl(io::Printer* printer, SourceLocation location) {
  std::string comments = location.leading_comments.empty()
                             ? location.trailing_comments
                             : location.leading_comments;
  if (comments.empty()) {
    return;
  }
  // XML escaping... no need for apostrophes etc as the whole text is going to
  // be a child node of a summary element, not part of an attribute.
  comments = absl::StrReplaceAll(comments, {{"&", "&amp;"}, {"<", "&lt;"}});
  std::vector<std::string> lines;
  lines = absl::StrSplit(comments, "\n", absl::AllowEmpty());
  // TODO: We really should work out which part to put in the summary and which
  // to put in the remarks... but that needs to be part of a bigger effort to
  // understand the markdown better anyway.
  printer->Print("/// <summary>\n");
  bool last_was_empty = false;
  // We squash multiple blank lines down to one, and remove any trailing blank
  // lines. We need to preserve the blank lines themselves, as this is relevant
  // in the markdown. Note that we can't remove leading or trailing whitespace
  // as *that's* relevant in markdown too. (We don't skip "just whitespace"
  // lines, either.)
  for (std::vector<std::string>::iterator it = lines.begin(); it != lines.end();
       ++it) {
    std::string line = *it;
    if (line.empty()) {
      last_was_empty = true;
    } else {
      if (last_was_empty) {
        printer->Print("///\n");
      }
      last_was_empty = false;
      // If the comment has an extra slash at the start then this can cause the
      // C# compiler to complain when generating the XML documentation Issue
      // https://github.com/grpc/grpc/issues/35905
      if (line[0] == '/') {
        line.replace(0, 1, "&#x2F;");
      }
      printer->Print("///$line$\n", "line", line);
    }
  }
  printer->Print("/// </summary>\n");
}

static std::string EncodeSourcePath(const std::string& path) {
  constexpr char kHex[] = "0123456789ABCDEF";
  std::string encoded;
  for (unsigned char character : path) {
    if ((character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9') || character == '-' ||
        character == '_' || character == '.' || character == '~' ||
        character == '/') {
      encoded.push_back(character);
      continue;
    }
    encoded.push_back('%');
    encoded.push_back(kHex[character >> 4]);
    encoded.push_back(kHex[character & 0x0f]);
  }
  return encoded;
}

static std::string EscapeSourceXml(const std::string& value) {
  return absl::StrReplaceAll(value, {{"&", "&amp;"},
                                     {"<", "&lt;"},
                                     {">", "&gt;"},
                                     {"\"", "&quot;"},
                                     {"'", "&apos;"}});
}

static void WriteSourceLink(io::Printer* printer, const Options* options,
                            const std::string& path, int start_line) {
  if (!options->source_links && !options->source_links_text_prefix) return;
  const std::string line = absl::StrCat(start_line + 1);
  const std::string text = EscapeSourceXml(absl::StrCat(
      options->source_links_text_prefix.value_or(""), path, ":", line));
  if (!options->source_links) {
    printer->Print("/// <remarks>Source: $text$</remarks>\n", "text", text);
    return;
  }
  const std::string encoded_path = EncodeSourcePath(path);
  const std::string href = absl::StrReplaceAll(
      *options->source_links, {{"{path}", encoded_path}, {"{line}", line}});
  printer->Print(
      "/// <remarks>Source: <see href=\"$href$\">$text$</see></remarks>\n",
      "href", EscapeSourceXml(href), "text", text);
}

template <typename DescriptorType>
static void WriteDocCommentBody(io::Printer* printer, const Options* options,
                                const DescriptorType* descriptor) {
  SourceLocation location;
  if (descriptor->GetSourceLocation(&location)) {
    WriteDocCommentBodyImpl(printer, location);
    WriteSourceLink(printer, options, descriptor->file()->name(),
                    location.start_line);
  }
}

void WriteMessageDocComment(io::Printer* printer, const Options* options,
                            const Descriptor* message) {
  if (options->strip_nonfunctional_codegen) return;
  WriteDocCommentBody(printer, options, message);
}

void WritePropertyDocComment(io::Printer* printer, const Options* options,
                             const FieldDescriptor* field) {
  if (options->strip_nonfunctional_codegen) return;
  WriteDocCommentBody(printer, options, field);
}

void WriteEnumDocComment(io::Printer* printer, const Options* options,
                         const EnumDescriptor* enumDescriptor) {
  if (options->strip_nonfunctional_codegen) return;
  WriteDocCommentBody(printer, options, enumDescriptor);
}
void WriteEnumValueDocComment(io::Printer* printer, const Options* options,
                              const EnumValueDescriptor* value) {
  if (options->strip_nonfunctional_codegen) return;
  WriteDocCommentBody(printer, options, value);
}

void WriteMethodDocComment(io::Printer* printer, const Options* options,
                           const MethodDescriptor* method) {
  if (options->strip_nonfunctional_codegen) return;
  WriteDocCommentBody(printer, options, method);
}

}  // namespace csharp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
