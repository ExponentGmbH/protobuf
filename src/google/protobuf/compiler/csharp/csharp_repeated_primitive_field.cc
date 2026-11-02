// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "google/protobuf/compiler/csharp/csharp_repeated_primitive_field.h"

#include <sstream>

#include "google/protobuf/compiler/code_generator.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/wire_format.h"
#include "google/protobuf/compiler/csharp/csharp_doc_comment.h"
#include "google/protobuf/compiler/csharp/csharp_helpers.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/descriptor_utils.h"

namespace google {
namespace protobuf {
namespace compiler {
namespace csharp {

RepeatedPrimitiveFieldGenerator::RepeatedPrimitiveFieldGenerator(
    const FieldDescriptor* descriptor, int presenceIndex, const Options *options)
    : FieldGeneratorBase(descriptor, presenceIndex, options) {
  variables_["setter"] = FieldInsideReferenceContainer(*descriptor_) ? "init" : "set";
  variables_["repeated_type"] = RepeatedFieldType(*descriptor_);
  variables_["repeated_type_classname"] = RepeatedFieldTypeClassName(*descriptor_);
  // A struct cannot initialize its field, and default(ImmutableArray<T>) throws when used, so null stands for empty.
  variables_["repeated_read"] = FieldInsideReferenceContainer(*descriptor_)
      ? absl::StrCat(name(), "_")
      : absl::StrCat("(", name(), "_ ?? ", RepeatedFieldType(*descriptor_), "<", variables_["type_name"], ">.Empty)");
}

RepeatedPrimitiveFieldGenerator::~RepeatedPrimitiveFieldGenerator() {

}

void RepeatedPrimitiveFieldGenerator::GenerateMembers(io::Printer* printer) {
  printer->Print(
    variables_,
    "private static readonly pb::FieldCodec<$type_name$> _repeated_$name$_codec\n"
    "    = pb::FieldCodec.For$capitalized_type_name$($tag$);\n");
  if (FieldInsideReferenceContainer(*descriptor_)) {
    printer->Print(
      variables_,
      "private $repeated_type$<$type_name$> $name$_ = $repeated_type$<$type_name$>.Empty;\n");
  } else {
    printer->Print(
      variables_,
      "private $repeated_type$<$type_name$>? $name$_;\n");
    printer->Print(
        variables_,
        "public static $repeated_type$<$type_name$> __$property_name$($extended_type$ message) { return message.$property_name$; }\n");
  }
  WritePropertyDocComment(printer, options(), descriptor_);
  AddPublicMemberAttributes(printer);
  printer->Print(
    variables_,
    "$access_level$ $repeated_type$<$type_name$> $property_name$ {\n"
    "  get { return $repeated_read$; }\n"
    "  $setter$ { $name$_ = value; }\n"
    "}\n"
    "private pbc::RepeatedField<$type_name$> $name$_ForSerialization { get { return pbc::RepeatedField<$type_name$>.From($repeated_read$); } }\n"
    );
}

void RepeatedPrimitiveFieldGenerator::GenerateMergingCode(io::Printer* printer) {
  printer->Print(
    variables_,
    "if (!other.$property_name$.IsEmpty) { $name$_ = $property_name$.AddRange(other.$property_name$); }\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateParsingCode(io::Printer* printer) {
  GenerateParsingCode(printer, true);
}

void RepeatedPrimitiveFieldGenerator::GenerateParsingCode(io::Printer* printer, bool use_parse_context) {
   printer->Print(
      variables_,
      use_parse_context
          ? "($name$_mutation ??= new pbc::RepeatedField<$type_name$>()).AddEntriesFrom(ref input, _repeated_$name$_codec);\n"
          : "($name$_mutation ??= new pbc::RepeatedField<$type_name$>()).AddEntriesFrom(input, _repeated_$name$_codec);\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateParseLoopPrologue(io::Printer* printer) {
  // Runs of this field may be split by other fields on the wire, and each run appends. Appending to the immutable
  // collection per run would make a hostile message of alternating fields quadratic, so they are collected first.
  printer->Print(variables_, "pbc::RepeatedField<$type_name$>? $name$_mutation = null;\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateParseLoopEpilogue(io::Printer* printer) {
  printer->Print(variables_, "if ($name$_mutation != null) { $name$_ = $property_name$.AddRange($name$_mutation); }\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateSerializationCode(io::Printer* printer) {
  GenerateSerializationCode(printer, true);
}

void RepeatedPrimitiveFieldGenerator::GenerateSerializationCode(io::Printer* printer, bool use_write_context) {
  printer->Print(
    variables_,
    use_write_context
    ? "$name$_ForSerialization.WriteTo(ref output, _repeated_$name$_codec);\n"
    : "$name$_ForSerialization.WriteTo(output, _repeated_$name$_codec);\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateSerializedSizeCode(io::Printer* printer) {
  printer->Print(
    variables_,
    "size += pbc::RepeatedField<$type_name$>.RepeatedFieldCalculateSize(_repeated_$name$_codec, $property_name$);\n");
}

void RepeatedPrimitiveFieldGenerator::WriteHash(io::Printer* printer) {
  printer->Print(
    variables_,
    "hash ^= pbc::RepeatedField<$type_name$>.GetRepeatedFieldHashCode($property_name$);\n");
}
void RepeatedPrimitiveFieldGenerator::WriteEquals(io::Printer* printer) {
  printer->Print(
    variables_,
    "if(!pbc::RepeatedField<$type_name$>.RepeatedFieldEquals($property_name$, other.$property_name$)) return false;\n");
}
void RepeatedPrimitiveFieldGenerator::WriteToString(io::Printer* printer) {
  printer->Print(variables_,
    "PrintField(\"$descriptor_name$\", $name$_ForSerialization, writer);\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateCloningCode(io::Printer* printer) {
  printer->Print(variables_,
    "if (deep) { $name$_ = other.$name$_ForSerialization.Clone().To$repeated_type_classname$(); } else { $name$_ = other.$name$_; }\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateFreezingCode(io::Printer* printer) {
}

void RepeatedPrimitiveFieldGenerator::GenerateExtensionCode(io::Printer* printer) {
  WritePropertyDocComment(printer, options(), descriptor_);
  AddDeprecatedFlag(printer);
  printer->Print(
    variables_,
    "$access_level$ static readonly pb::RepeatedExtension<$extended_type$, $type_name$> $property_name$ =\n"
    "  new pb::RepeatedExtension<$extended_type$, $type_name$>($number$, pb::FieldCodec.For$capitalized_type_name$($tag$));\n");
}

void RepeatedPrimitiveFieldGenerator::GenerateStructConstructorCode(io::Printer *printer) {
  printer->Print(variables_,
                 "$name$_ = default;\n");
}

}  // namespace csharp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
