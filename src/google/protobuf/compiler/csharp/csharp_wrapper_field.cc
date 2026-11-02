// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "google/protobuf/compiler/csharp/csharp_wrapper_field.h"

#include <sstream>

#include "google/protobuf/compiler/code_generator.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/compiler/csharp/csharp_doc_comment.h"
#include "google/protobuf/compiler/csharp/csharp_helpers.h"
#include "google/protobuf/compiler/csharp/csharp_options.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/descriptor_utils.h"

namespace google {
namespace protobuf {
namespace compiler {
namespace csharp {

WrapperFieldGenerator::WrapperFieldGenerator(const FieldDescriptor* descriptor,
                                       int presenceIndex, const Options *options)
    : FieldGeneratorBase(descriptor, presenceIndex, options) {
  variables_["has_property_check"] = absl::StrCat(name(), "_ != null");
  variables_["has_not_property_check"] = absl::StrCat(name(), "_ == null");
  // Reads the field rather than the property, so that the compiler connects it to has_property_check.
  variables_["checked_property"] = absl::StrCat(name(), "_");
  // The wrapper of a value type is `int?` already, where the common `type_name?` would make `int??`.
  const std::string type = type_name();
  variables_["type_at_rest"] = absl::EndsWith(type, "?") ? type : absl::StrCat(type, "?");
  variables_["property_setter"] = FieldInsideReferenceContainer(*descriptor) ? "init" : "set";
  const FieldDescriptor* wrapped_field = descriptor->message_type()->field(0);
  is_value_type = wrapped_field->type() != FieldDescriptor::TYPE_STRING &&
      wrapped_field->type() != FieldDescriptor::TYPE_BYTES;
  if (is_value_type) {
    variables_["nonnullable_type_name"] = type_name(wrapped_field);
  }
}

WrapperFieldGenerator::~WrapperFieldGenerator() {
}

void WrapperFieldGenerator::GenerateMembers(io::Printer* printer) {
  printer->Print(
        variables_,
        "private static readonly pb::FieldCodec<$type_name$> _single_$name$_codec = ");
  GenerateCodecCode(printer);
  printer->Print(
    variables_,
    ";\n"
    "private $type_at_rest$ $name$_;\n");
  if (!FieldInsideReferenceContainer(*descriptor_)) {
    // FieldDescriptor builds the accessor of a struct's field from these: a property delegate cannot take a struct.
    printer->Print(
      variables_,
      "public static $type_at_rest$ __$property_name$($extended_type$ message) { return message.$property_name$; }\n"
      "public static bool __$property_name$_has_value($extended_type$ message) { return message.$property_name$ != null; }\n");
  }
  WritePropertyDocComment(printer, options(), descriptor_);
  AddPublicMemberAttributes(printer);
  printer->Print(
    variables_,
    "$access_level$ $type_at_rest$ $property_name$ {\n"
    "  get { return $name$_; }\n"
    "  $property_setter$ { $property_name$_Internal = value; }\n"
    "}\n"
    "private $type_at_rest$ $property_name$_Internal {\n"
    "  set { $name$_ = value; }\n"
    "}\n\n");
  if (SupportsPresenceApi(descriptor_)) {
    printer->Print(
      variables_,
      "/// <summary>Gets whether the $descriptor_name$ field is set</summary>\n");
    AddPublicMemberAttributes(printer);
    printer->Print(
      variables_,
      "$access_level$ bool Has$property_name$ {\n"
      "  get { return $name$_ != null; }\n"
      "}\n\n");
    printer->Print(
      variables_,
      "/// <summary>Clears the value of the $descriptor_name$ field</summary>\n");
    AddPublicMemberAttributes(printer);
    printer->Print(
      variables_,
      "private void Clear$property_name$() {\n"
      "  $name$_ = null;\n"
      "}\n");
  }
}

void WrapperFieldGenerator::GenerateMergingCode(io::Printer* printer) {
  printer->Print(
    variables_,
    "if (other.$has_property_check$) {\n"
    "  if ($has_not_property_check$ || other.$property_name$ != $default_value$) {\n"
    "    $property_name$_Internal = other.$property_name$;\n"
    "  }\n"
    "}\n");
}

void WrapperFieldGenerator::GenerateParsingCode(io::Printer* printer) {
  GenerateParsingCode(printer, true);
}

void WrapperFieldGenerator::GenerateParsingCode(io::Printer* printer, bool use_parse_context) {
  printer->Print(
    variables_,
    use_parse_context
    ? "$type_name$ value = _single_$name$_codec.Read(ref input);\n"
      "if ($has_not_property_check$ || value != $default_value$) {\n"
      "  $property_name$_Internal = value;\n"
      "}\n"
    : "$type_name$ value = _single_$name$_codec.Read(input);\n"
      "if ($has_not_property_check$ || value != $default_value$) {\n"
      "  $property_name$_Internal = value;\n"
      "}\n");
}

void WrapperFieldGenerator::GenerateSerializationCode(io::Printer* printer) {
  GenerateSerializationCode(printer, true);
}

void WrapperFieldGenerator::GenerateSerializationCode(io::Printer* printer, bool use_write_context) {
  printer->Print(
    variables_,
    use_write_context
    ? "if ($has_property_check$) {\n"
      "  _single_$name$_codec.WriteTagAndValue(ref output, $checked_property$);\n"
      "}\n"
    : "if ($has_property_check$) {\n"
      "  _single_$name$_codec.WriteTagAndValue(output, $checked_property$);\n"
      "}\n");
}

void WrapperFieldGenerator::GenerateSerializedSizeCode(io::Printer* printer) {
  printer->Print(
    variables_,
    "if ($has_property_check$) {\n"
    "  size += _single_$name$_codec.CalculateSizeWithTag($checked_property$);\n"
    "}\n");
}

void WrapperFieldGenerator::WriteHash(io::Printer* printer) {
  const char *text = "if ($has_property_check$) hash ^= $checked_property$.GetHashCode();\n";
  if (descriptor_->message_type()->field(0)->type() == FieldDescriptor::TYPE_FLOAT) {
    text = "if ($has_property_check$) hash ^= pbc::ProtobufEqualityComparers.BitwiseNullableSingleEqualityComparer.GetHashCode($checked_property$);\n";
  }
  else if (descriptor_->message_type()->field(0)->type() == FieldDescriptor::TYPE_DOUBLE) {
    text = "if ($has_property_check$) hash ^= pbc::ProtobufEqualityComparers.BitwiseNullableDoubleEqualityComparer.GetHashCode($checked_property$);\n";
  }
  printer->Print(variables_, text);
}

void WrapperFieldGenerator::WriteEquals(io::Printer* printer) {
  const char *text = "if ($property_name$ != other.$property_name$) return false;\n";
  if (descriptor_->message_type()->field(0)->type() == FieldDescriptor::TYPE_FLOAT) {
    text = "if (!pbc::ProtobufEqualityComparers.BitwiseNullableSingleEqualityComparer.Equals($property_name$, other.$property_name$)) return false;\n";
  }
  else if (descriptor_->message_type()->field(0)->type() == FieldDescriptor::TYPE_DOUBLE) {
    text = "if (!pbc::ProtobufEqualityComparers.BitwiseNullableDoubleEqualityComparer.Equals($property_name$, other.$property_name$)) return false;\n";
  }
  printer->Print(variables_, text);
}

void WrapperFieldGenerator::WriteToString(io::Printer* printer) {
  // TODO: Implement if we ever actually need it...
}

void WrapperFieldGenerator::GenerateCloningCode(io::Printer* printer) {
  printer->Print(variables_,
    "$property_name$_Internal = other.$property_name$;\n");
}

void WrapperFieldGenerator::GenerateCodecCode(io::Printer* printer) {
  if (is_value_type) {
    printer->Print(
      variables_,
      "pb::FieldCodec.ForStructWrapper<$nonnullable_type_name$>($tag$)");
  } else {
    printer->Print(
      variables_,
      "pb::FieldCodec.ForClassWrapper<$type_name$>($tag$)");
  }
}

void WrapperFieldGenerator::GenerateExtensionCode(io::Printer* printer) {
  WritePropertyDocComment(printer, options(), descriptor_);
  AddDeprecatedFlag(printer);
  printer->Print(
    variables_,
    "$access_level$ static readonly pb::Extension<$extended_type$, $type_name$> $property_name$ =\n"
    "  new pb::Extension<$extended_type$, $type_name$>($number$, ");
  GenerateCodecCode(printer);
  printer->Print(");\n");
}

void WrapperFieldGenerator::GenerateStructConstructorCode(io::Printer *printer) {
  printer->Print(variables_, "$name$_ = default;\n");
}


WrapperOneofFieldGenerator::WrapperOneofFieldGenerator(
    const FieldDescriptor* descriptor, int presenceIndex, const Options *options)
    : WrapperFieldGenerator(descriptor, presenceIndex, options) {
    SetCommonOneofFieldVariables(&variables_);
    variables_["has_property_check"] = variables_["has_property_check_internal"];
    // Every use follows has_property_check, which compares the case; the compiler cannot connect that to the property.
    variables_["checked_property"] = absl::StrCat(property_name(), "!");
}

WrapperOneofFieldGenerator::~WrapperOneofFieldGenerator() {
}

void WrapperOneofFieldGenerator::GenerateMembers(io::Printer* printer) {
  // Note: deliberately _oneof_$name$_codec, not _$oneof_name$_codec... we have one codec per field.
  printer->Print(
        variables_,
        "private static readonly pb::FieldCodec<$type_name$> _oneof_$name$_codec = ");
  GenerateCodecCode(printer);
  printer->Print(";\n");
  GenerateOneofCaseMembers(printer);
}

void WrapperOneofFieldGenerator::GenerateMergingCode(io::Printer* printer) {
  printer->Print(variables_, "$property_name$_Internal = other.$property_name$;\n");
}

void WrapperOneofFieldGenerator::GenerateParsingCode(io::Printer* printer) {
  GenerateParsingCode(printer, true);
}

void WrapperOneofFieldGenerator::GenerateParsingCode(io::Printer* printer, bool use_parse_context) {
  printer->Print(
    variables_,
    use_parse_context
    ? "$property_name$_Internal = _oneof_$name$_codec.Read(ref input);\n"
    : "$property_name$_Internal = _oneof_$name$_codec.Read(input);\n");
}

void WrapperOneofFieldGenerator::GenerateSerializationCode(io::Printer* printer) {
  GenerateSerializationCode(printer, true);
}

void WrapperOneofFieldGenerator::GenerateSerializationCode(io::Printer* printer, bool use_write_context) {
  // TODO: I suspect this is wrong...
  printer->Print(
    variables_,
    use_write_context
    ? "if ($has_property_check$) {\n"
      "  _oneof_$name$_codec.WriteTagAndValue(ref output, $checked_property$);\n"
      "}\n"
    : "if ($has_property_check$) {\n"
      "  _oneof_$name$_codec.WriteTagAndValue(output, $checked_property$);\n"
      "}\n");
}

void WrapperOneofFieldGenerator::GenerateSerializedSizeCode(io::Printer* printer) {
  // TODO: I suspect this is wrong...
  printer->Print(
    variables_,
    "if ($has_property_check$) {\n"
    "  size += _oneof_$name$_codec.CalculateSizeWithTag($checked_property$);\n"
    "}\n");
}

}  // namespace csharp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
