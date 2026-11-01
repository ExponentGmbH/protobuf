// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/compiler/code_generator.h"
#include "google/protobuf/compiler/csharp/csharp_doc_comment.h"
#include "google/protobuf/compiler/csharp/csharp_generator.h"
#include "google/protobuf/compiler/parser.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/io/tokenizer.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/wrappers.pb.h"

namespace google {
namespace protobuf {
namespace compiler {
namespace csharp {
namespace {

class SourceLinkContext : public GeneratorContext {
 public:
  io::ZeroCopyOutputStream* Open(const std::string& filename) override {
    return new io::StringOutputStream(&files[filename]);
  }

  std::map<std::string, std::string> files;
};

class CSharpSourceLinksTest : public testing::Test {
 public:
  void SetUp() override {
    StringValue::descriptor();
    io::ArrayInputStream input(kProto, sizeof(kProto) - 1);
    io::Tokenizer tokenizer(&input, nullptr);
    Parser parser;
    ASSERT_TRUE(parser.Parse(&tokenizer, &proto_));
    proto_.set_name("holosweat/proto/client/db/user.proto");
  }

  std::string Generate(const std::string& parameters) {
    DescriptorPool pool(DescriptorPool::generated_pool());
    const FileDescriptor* file = pool.BuildFile(proto_);
    EXPECT_NE(file, nullptr);
    if (file == nullptr) return "";
    SourceLinkContext context;
    std::string error;
    EXPECT_TRUE(Generator().Generate(file, parameters, &context, &error))
        << error;
    EXPECT_EQ(context.files.size(), 1);
    return context.files.empty() ? "" : context.files.begin()->second;
  }

  // Returns the generator error, or "" when generation succeeds.
  std::string GenerateError(const std::string& parameters) {
    DescriptorPool pool(DescriptorPool::generated_pool());
    const FileDescriptor* file = pool.BuildFile(proto_);
    EXPECT_NE(file, nullptr);
    if (file == nullptr) return "";
    SourceLinkContext context;
    std::string error;
    if (Generator().Generate(file, parameters, &context, &error)) return "";
    EXPECT_TRUE(context.files.empty());
    return error;
  }

 protected:
  FileDescriptorProto proto_;
  static constexpr char kTemplate[] =
      "vscode://codepit.codepit-vscode/open-source?"
      "path=Assets/holosweat/api/{path}&line={line}";

 private:
  static constexpr char kProto[] =
      "syntax = \"proto3\";\n"
      "package source_links;\n"
      "import \"google/protobuf/wrappers.proto\";\n"
      "// User preferences\n"
      "message User {\n"
      "  // A documented field.\n"
      "  string commented = 1;\n"
      "  string plain = 2;\n"
      "  repeated string tags = 3;\n"
      "  map<string, int32> counts = 4;\n"
      "  google.protobuf.StringValue wrapper = 5;\n"
      "  oneof choice {\n"
      "    string chosen = 6;\n"
      "    User chosen_user = 7;\n"
      "    Status chosen_status = 8;\n"
      "    google.protobuf.StringValue chosen_wrapper = 9;\n"
      "  }\n"
      "  Status status = 10;\n"
      "  User child = 11;\n"
      "  repeated Status statuses = 12;\n"
      "  repeated User children = 13;\n"
      "  message Nested {}\n"
      "}\n"
      "enum Status {\n"
      "  STATUS_UNSPECIFIED = 0;\n"
      "  // Active user.\n"
      "  STATUS_ACTIVE = 1;\n"
      "}\n"
      "service Users {\n"
      "  // Reads a user.\n"
      "  rpc Get(User) returns (User);\n"
      "}\n";
};

TEST_F(CSharpSourceLinksTest, ParameterParserPreservesExactTemplate) {
  std::vector<std::pair<std::string, std::string>> parameters;
  ParseGeneratorParameter(std::string("source_links=") + kTemplate +
                              ",source_links_text_prefix=Assets/holosweat/api/",
                          &parameters);
  ASSERT_EQ(parameters.size(), 2);
  EXPECT_EQ(parameters[0].first, "source_links");
  EXPECT_EQ(parameters[0].second, kTemplate);
  EXPECT_EQ(parameters[1].first, "source_links_text_prefix");
  EXPECT_EQ(parameters[1].second, "Assets/holosweat/api/");
}

TEST_F(CSharpSourceLinksTest,
       LinksEveryGeneratedDeclarationAndPreservesDefault) {
  const std::string baseline = Generate("");
  std::string linked =
      Generate(std::string("source_links=") + kTemplate +
               ",source_links_text_prefix=Assets/holosweat/api/");
  struct Declaration {
    const char* name;
    int line;
  };
  const Declaration declarations[] = {
      {"User", 5},           {"Commented", 7},    {"Plain", 8},
      {"Tags", 9},           {"Counts", 10},      {"Wrapper", 11},
      {"Chosen", 13},        {"ChosenUser", 14},  {"ChosenStatus", 15},
      {"ChosenWrapper", 16}, {"Status", 18},      {"Child", 19},
      {"Statuses", 20},      {"Children", 21},    {"Nested", 22},
      {"Status enum", 24},   {"Unspecified", 25}, {"Active", 27},
  };
  for (const Declaration& declaration : declarations) {
    SCOPED_TRACE(declaration.name);
    const std::string line = std::to_string(declaration.line);
    const std::string remarks =
        "/// <remarks>Source: <see href=\"vscode://codepit.codepit-vscode/"
        "open-source?path=Assets/holosweat/api/holosweat/proto/client/db/"
        "user.proto&amp;line=" +
        line +
        "\">Assets/holosweat/api/holosweat/proto/client/db/user.proto:" + line +
        "</see></remarks>\n";
    const size_t position = linked.find(remarks);
    ASSERT_NE(position, std::string::npos);
    const size_t line_start = linked.rfind('\n', position);
    ASSERT_NE(line_start, std::string::npos);
    linked.erase(line_start + 1, position + remarks.size() - line_start - 1);
  }
  // Source links are the only permitted output difference.
  EXPECT_EQ(linked, baseline);
  EXPECT_NE(baseline.find("/// A documented field."), std::string::npos);
  EXPECT_EQ(baseline.find("<remarks>Source:"), std::string::npos);
}

TEST_F(CSharpSourceLinksTest, PrefixWithoutTemplateProducesPlainText) {
  const std::string generated =
      Generate("source_links_text_prefix=Assets/holosweat/api/");
  EXPECT_NE(
      generated.find("/// <remarks>Source: Assets/holosweat/api/holosweat/"
                     "proto/client/db/user.proto:8</remarks>"),
      std::string::npos);
  EXPECT_EQ(generated.find("<see href="), std::string::npos);
}

TEST_F(CSharpSourceLinksTest, TemplateWithoutPrefixKeepsProtoRelativeText) {
  const std::string generated =
      Generate(std::string("source_links=") + kTemplate);
  EXPECT_NE(generated.find("&amp;line=8\">holosweat/proto/client/db/"
                           "user.proto:8</see></remarks>"),
            std::string::npos);
}

TEST_F(CSharpSourceLinksTest, EmptyPrefixEnablesPlainProtoRelativeText) {
  const std::string generated = Generate("source_links_text_prefix=");
  EXPECT_NE(generated.find("/// <remarks>Source: holosweat/proto/client/db/"
                           "user.proto:8</remarks>"),
            std::string::npos);
  EXPECT_EQ(generated.find("<see href="), std::string::npos);
}

// An empty template (e.g. from an unset shell variable) or a misspelled
// placeholder would otherwise ship links that all point to the same place.
TEST_F(CSharpSourceLinksTest, RejectsTemplateWithoutBothPlaceholders) {
  for (const std::string parameter :
       {"source_links=", "source_links=vscode://x?line={line}",
        "source_links=vscode://x?path={path}",
        "source_links=vscode://x?path={Path}&line={line}"}) {
    SCOPED_TRACE(parameter);
    EXPECT_NE(GenerateError(parameter).find(
                  "source_links must contain {path} and {line}"),
              std::string::npos);
  }
}

TEST_F(CSharpSourceLinksTest, EncodesPathSegmentsAndEscapesXml) {
  proto_.set_name("folder \xc3\xa9 &\"'<>%{line}/user.proto");
  const std::string generated =
      Generate(std::string("source_links=") + kTemplate +
               "&label=\"'<x>,source_links_text_prefix=Assets &\"'<>/");
  EXPECT_NE(
      generated.find("path=Assets/holosweat/api/"
                     "folder%20%C3%A9%20%26%22%27%3C%3E%25%7Bline%7D/"
                     "user.proto&amp;line=8&amp;label=&quot;&apos;&lt;x&gt;\">"
                     "Assets &amp;&quot;&apos;&lt;&gt;/folder \xc3\xa9 "
                     "&amp;&quot;&apos;&lt;&gt;"
                     "%{line}/user.proto:8</see></remarks>"),
      std::string::npos);
}

TEST_F(CSharpSourceLinksTest, StrippedCodegenIgnoresBothOptions) {
  const std::string baseline =
      Generate("experimental_strip_nonfunctional_codegen");
  const std::string linked = Generate(
      std::string("experimental_strip_nonfunctional_codegen,source_links=") +
      kTemplate + ",source_links_text_prefix=Assets/holosweat/api/");
  EXPECT_EQ(linked, baseline);
  EXPECT_EQ(linked.find("<remarks>Source:"), std::string::npos);
}

TEST_F(CSharpSourceLinksTest, MissingSourceLocationsDoNotInventLineNumbers) {
  proto_.clear_source_code_info();
  const std::string baseline = Generate("");
  EXPECT_EQ(Generate(std::string("source_links=") + kTemplate), baseline);
}

TEST_F(CSharpSourceLinksTest, MethodCommentHelperUsesSameOptionsAndStripGuard) {
  DescriptorPool pool(DescriptorPool::generated_pool());
  const FileDescriptor* file = pool.BuildFile(proto_);
  ASSERT_NE(file, nullptr);
  const MethodDescriptor* method = file->service(0)->method(0);
  for (bool stripped : {false, true}) {
    SCOPED_TRACE(stripped);
    Options options;
    options.source_links = kTemplate;
    options.source_links_text_prefix = "Assets/holosweat/api/";
    options.strip_nonfunctional_codegen = stripped;
    std::string generated;
    {
      io::StringOutputStream output(&generated);
      io::Printer printer(&output, '$');
      WriteMethodDocComment(&printer, &options, method);
    }
    const std::string expected =
        stripped
            ? ""
            : "/// <summary>\n/// Reads a user.\n/// </summary>\n"
              "/// <remarks>Source: <see "
              "href=\"vscode://codepit.codepit-vscode/"
              "open-source?path=Assets/holosweat/api/holosweat/proto/client/db/"
              "user.proto&amp;line=31\">Assets/holosweat/api/holosweat/proto/"
              "client/db/"
              "user.proto:31</see></remarks>\n";
    EXPECT_EQ(generated, expected);
  }
}

}  // namespace
}  // namespace csharp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
