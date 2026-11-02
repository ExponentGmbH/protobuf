// Protocol Buffers - Google's data interchange format
// Copyright 2015 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "google/protobuf/compiler/csharp/csharp_map_field.h"

#include <sstream>

#include "google/protobuf/compiler/code_generator.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/compiler/csharp/csharp_doc_comment.h"
#include "google/protobuf/compiler/csharp/csharp_helpers.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/descriptor_utils.h"

namespace google {
namespace protobuf {
namespace compiler {
namespace csharp {

MapFieldGenerator::MapFieldGenerator(const FieldDescriptor* descriptor,
                                     int presenceIndex,
                                     const Options* options)
    : FieldGeneratorBase(descriptor, presenceIndex, options) {
  const FieldDescriptor* key_descriptor =
      descriptor_->message_type()->map_key();
  const FieldDescriptor* value_descriptor =
      descriptor_->message_type()->map_value();
  variables_["key_type_name"] = type_name(key_descriptor);
  variables_["value_type_name"] = type_name(value_descriptor);
  variables_["setter"] = FieldInsideReferenceContainer(*descriptor_) ? "init" : "set";
}

MapFieldGenerator::~MapFieldGenerator() {
}

void MapFieldGenerator::GenerateMembers(io::Printer* printer) {
    const FieldDescriptor* key_descriptor =
      descriptor_->message_type()->map_key();
  const FieldDescriptor* value_descriptor =
      descriptor_->message_type()->map_value();
  std::unique_ptr<FieldGeneratorBase> key_generator(
      CreateFieldGenerator(key_descriptor, 1, this->options()));
  std::unique_ptr<FieldGeneratorBase> value_generator(
      CreateFieldGenerator(value_descriptor, 2, this->options()));

  printer->Print(
    variables_,
    "private static readonly pbc::MapField<$key_type_name$, $value_type_name$>.Codec _map_$name$_codec\n"
    "    = new pbc::MapField<$key_type_name$, $value_type_name$>.Codec(");
  key_generator->GenerateCodecCode(printer);
  printer->Print(", ");
  value_generator->GenerateCodecCode(printer);
  if (FieldInsideReferenceContainer(*descriptor_)) {
    printer->Print(
      variables_,
      ", $tag$);\n"
      "private pbc::MapField<$key_type_name$, $value_type_name$>? $name$_pb = null;\n"
      "private scg.IReadOnlyDictionary<$key_type_name$, $value_type_name$>? $name$_imm = null;\n");
  } else {
    printer->Print(
      variables_,
      ", $tag$);\n"
      "private pbc::MapField<$key_type_name$, $value_type_name$>? $name$_pb;\n"
      "private scg.IReadOnlyDictionary<$key_type_name$, $value_type_name$>? $name$_imm;\n");
    printer->Print(
      variables_,
      "public static scg.IReadOnlyDictionary<$key_type_name$, $value_type_name$> __$property_name$($extended_type$ message) { return message.$property_name$; }\n");
  }
  WritePropertyDocComment(printer, options(), descriptor_);
  AddPublicMemberAttributes(printer);
  // A parsed map lives in a MapField, whose object must not be handed out: a cast would reach its Add and Clear.
  printer->Print(
      variables_,
      "$access_level$ scg::IReadOnlyDictionary<$key_type_name$, $value_type_name$> $property_name$ {\n"
      "  get { return $name$_imm ?? ($name$_pb != null ? (scg::IReadOnlyDictionary<$key_type_name$, $value_type_name$>) new global::System.Collections.ObjectModel.ReadOnlyDictionary<$key_type_name$, $value_type_name$>($name$_pb) : sci.ImmutableDictionary<$key_type_name$, $value_type_name$>.Empty); }\n"
      "  $setter$ { if (value is sci.ImmutableDictionary<$key_type_name$, $value_type_name$>) { $name$_imm = value; } else { $name$_imm = sci.ImmutableDictionary.CreateRange(value); } $name$_pb = null; }\n"
      "}\n"
      // Size, hash, equality and merge read through this, so that they don't allocate the public getter's wrapper.
      "private scg::IReadOnlyDictionary<$key_type_name$, $value_type_name$> $name$_ForReading { get { return $name$_imm ?? (scg::IReadOnlyDictionary<$key_type_name$, $value_type_name$>?) $name$_pb ?? sci.ImmutableDictionary<$key_type_name$, $value_type_name$>.Empty; } }\n"
      "private pbc::MapField<$key_type_name$, $value_type_name$> $name$_ForSerialization { get { return $name$_imm != null || $name$_pb == null ? new pbc::MapField<$key_type_name$, $value_type_name$>($name$_imm ?? sci.ImmutableDictionary<$key_type_name$, $value_type_name$>.Empty) : $name$_pb; } }\n"
      "private pbc::MapField<$key_type_name$, $value_type_name$> $name$_ForMutation { get {  if ($name$_imm != null) { $name$_pb = new pbc::MapField<$key_type_name$, $value_type_name$>($name$_imm); $name$_imm = null; } if ($name$_pb == null) { $name$_pb = new pbc::MapField<$key_type_name$, $value_type_name$>(); } return $name$_pb; } }\n");
}

void MapFieldGenerator::GenerateMergingCode(io::Printer* printer) {
  printer->Print(variables_,
                 "if (other.$name$_ForReading.Count > 0) { var $name$_new = new pbc::MapField<$key_type_name$, $value_type_name$>($name$_ForReading); $name$_new.MergeFrom(other.$name$_ForReading); $name$_imm = null; $name$_pb = $name$_new; }\n");
}

void MapFieldGenerator::GenerateParsingCode(io::Printer* printer) {
  GenerateParsingCode(printer, true);
}

void MapFieldGenerator::GenerateParsingCode(io::Printer* printer, bool use_parse_context) {
  printer->Print(
    variables_,
    use_parse_context
    ? "$name$_ForMutation.AddEntriesFrom(ref input, _map_$name$_codec);\n"
    : "$name$_ForMutation.AddEntriesFrom(input, _map_$name$_codec);\n");
}

void MapFieldGenerator::GenerateParseLoopPrologue(io::Printer* printer) {
  // Copies of a message share its parsed MapField, which is what makes them cheap, so a parse takes its own first.
  printer->Print(variables_,
    "if ($name$_pb != null) { $name$_pb = new pbc::MapField<$key_type_name$, $value_type_name$>($name$_pb); }\n");
}

void MapFieldGenerator::GenerateSerializationCode(io::Printer* printer) {
  GenerateSerializationCode(printer, true);
}

void MapFieldGenerator::GenerateSerializationCode(io::Printer* printer, bool use_write_context) {
  printer->Print(
    variables_,
    use_write_context
    ? "$name$_ForSerialization.WriteTo(ref output, _map_$name$_codec);\n"
    : "$name$_ForSerialization.WriteTo(output, _map_$name$_codec);\n");
}

void MapFieldGenerator::GenerateSerializedSizeCode(io::Printer* printer) {
  printer->Print(
    variables_,
    "size += pbc::MapField<$key_type_name$, $value_type_name$>.MapFieldCalculateSize(_map_$name$_codec, $name$_ForReading);\n");
}

void MapFieldGenerator::WriteHash(io::Printer* printer) {
  printer->Print(
    variables_,
    "hash ^= pbc::MapField<$key_type_name$, $value_type_name$>.GetMapFieldHashCode($name$_ForReading);\n");
}
void MapFieldGenerator::WriteEquals(io::Printer* printer) {
  printer->Print(
    variables_,
    "if (!pbc::MapField<$key_type_name$, $value_type_name$>.MapFieldEquals(this.$name$_ForReading, other.$name$_ForReading)) return false;\n");
}

void MapFieldGenerator::WriteToString(io::Printer* printer) {
    // TODO: If we ever actually use ToString, we'll need to impleme this...
}

void MapFieldGenerator::GenerateCloningCode(io::Printer* printer) {
  printer->Print(variables_,
    "if (deep) { $name$_imm = null; $name$_pb = other.$name$_ForSerialization.Clone(); } else { $name$_imm = other.$name$_imm; $name$_pb = other.$name$_pb; }\n");
}

void MapFieldGenerator::GenerateFreezingCode(io::Printer* printer) {
}

void MapFieldGenerator::GenerateStructConstructorCode(io::Printer *printer) {
  printer->Print(variables_, "$name$_pb = default; $name$_imm = default;\n");
}

}  // namespace csharp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
