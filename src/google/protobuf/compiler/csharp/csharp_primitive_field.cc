// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "google/protobuf/compiler/csharp/csharp_primitive_field.h"

#include <sstream>
#include <string>
#include <utility>

#include "google/protobuf/compiler/code_generator.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/compiler/csharp/csharp_doc_comment.h"
#include "google/protobuf/compiler/csharp/csharp_helpers.h"
#include "google/protobuf/compiler/csharp/csharp_options.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/descriptor_utils.h"

namespace google {
namespace protobuf {
namespace compiler {
namespace csharp {

PrimitiveFieldGenerator::PrimitiveFieldGenerator(
    const FieldDescriptor* descriptor, int presenceIndex, const Options *options)
    : FieldGeneratorBase(descriptor, presenceIndex, options) {
  // TODO: Make this cleaner...
  is_value_type = descriptor->type() != FieldDescriptor::TYPE_STRING
      && descriptor->type() != FieldDescriptor::TYPE_BYTES;
  if (!is_value_type && !SupportsPresenceApi(descriptor_)) {
    // A struct's string and bytes fields are null until assigned: it cannot initialize them, and a parse skips absent ones.
    const std::string non_empty = FieldInsideReferenceContainer(*descriptor_) ? ".Length != 0" : "?.Length > 0";
    variables_["has_property_check"] = variables_["property_name"] + non_empty;
    variables_["other_has_property_check"] = "other." + variables_["property_name"] + non_empty;
    variables_["property_name_existing"] = variables_["property_name"];
    variables_["other_property_name_existing"] = "other." + variables_["property_name"];
  }
}

PrimitiveFieldGenerator::~PrimitiveFieldGenerator() {
}

void PrimitiveFieldGenerator::GenerateMembers(io::Printer* printer) {
  // Note: in multiple places, this code assumes that all fields
  // that support presence are either nullable, or use a presence field bit.
  // Fields which are oneof members are not generated here; they're generated in PrimitiveOneofFieldGenerator below.
  // Extensions are not generated here either.

  // Explicit presence allows different default values to be specified. These
  // are retained via static fields. They don't particularly need to be, but we
  // don't need to change that. Under implicit presence we don't use static
  // fields for default values and just use the literals instead.
  // if (descriptor_->has_presence()) {
  //   // Note: "private readonly static" isn't as idiomatic as
  //   // "private static readonly", but changing this now would create a lot of
  //   // churn in generated code with near-to-zero benefit.
  //   printer->Print(
  //     variables_,
  //     "private readonly static $type_name$ $property_name$DefaultValue = $default_value$;\n\n");
  //   std::string property_name = variables_["property_name"];
  //   variables_["default_value_access"] =
  //       absl::StrCat(property_name, "DefaultValue");
  // } else {
  //   std::string default_value = variables_["default_value"];
  //   variables_["default_value_access"] = std::move(default_value);
  // }

  if (EmbedBarePublicField(*descriptor_)) {
    WritePropertyDocComment(printer, options(), descriptor_);
    printer->Print(
          variables_,
          "public $type_name$  $property_name$;\n");
    printer->Print(
        variables_,
        "public static $type_at_rest$ __$property_name$($extended_type$ message) { return message.$property_name$; }\n");
    if (IsNullable(descriptor_) && SupportsPresenceApi(descriptor_)) {
            printer->Print(
        variables_,
        "public static bool __$property_name$_has_value($extended_type$ message) { return message.$property_name$ != null; }\n");
    }
    return;
  }

  // Declare the field itself.
  if (IsNullable(descriptor_) && SupportsPresenceApi(descriptor_)) {
      printer->Print(
          variables_,
          "private $type_name$? $name_def_message$;\n");
      printer->Print(
        variables_,
        "public static bool __$property_name$_has_value($extended_type$ message) { return message.$property_name$ != null; }\n");
  }
  else {
      printer->Print(
          variables_,
          "private $type_name$ $name_def_message$;\n");
  }

  if (!FieldInsideReferenceContainer(*descriptor_)) {
    printer->Print(
      variables_,
      "public static $type_at_rest$ __$property_name$($extended_type$ message) { return message.$property_name$; }\n");
  }

  WritePropertyDocComment(printer, options(), descriptor_);
  AddPublicMemberAttributes(printer);

  // Most of the work is done in the property:
  // Declare the property itself (the same for all options)
  if (SupportsPresenceApi(descriptor_)) {
    printer->Print(variables_, "$access_level$ $type_name$? $property_name$ {\n");
  } else {
    printer->Print(variables_, "$access_level$ $type_name$ $property_name$ {\n");
  }

  // Specify the "getter", which may need to check for a presence field.
  if (SupportsPresenceApi(descriptor_)) {
    if (IsNullable(descriptor_)) {
      printer->Print(
        variables_,
        "  get { return $name$_; }\n");
    } else {
      printer->Print(
        variables_,
        // Note: it's possible that this could be rewritten as a
        // conditional ?: expression, but there's no significant benefit
        // to changing it.
        "  get { if ($has_field_check$) { return $name$_; } else { return null; } }\n");
    }
  } else {
    printer->Print(
      variables_,
      "  get { return $name$_; }\n");
  }

  // Specify the "setter", which may need to set a field bit as well as the
  // value.
  if (FieldInsideReferenceContainer(*descriptor_)) {
    printer->Print("  init {\n");
  } else {
    printer->Print("  set {\n");
  }
  printer->Print(variables_, "    this.$property_name$_Internal = value;\n");
  printer->Print("  }\n");
  printer->Print("}\n");

  if (SupportsPresenceApi(descriptor_)) {
    printer->Print(variables_, "private $type_name$? $property_name$_Internal {\n");
  } else {
    printer->Print(variables_, "private $type_name$ $property_name$_Internal {\n");
  }

  printer->Print("  set {\n");
  if (is_value_type) {
    if (SupportsPresenceApi(descriptor_)) {
      printer->Print(
        variables_,
        "    if (value != null) { $set_has_field$; $name$_ = value.Value; } else {  $clear_has_field$; } \n");
    } else {
      printer->Print(
        variables_,
        "    $name$_ = value;\n");
    }
  } else {
    if (SupportsPresenceApi(descriptor_)) {
      printer->Print(
        variables_,
        "    $name$_ = value;\n");
    } else {
      printer->Print(
        variables_,
        "    $name$_ = pb::ProtoPreconditions.CheckNotNull(value, \"value\");\n");
    }
  }
  printer->Print(
    "  }\n"
    "}\n");

  // The "HasFoo" property, where required.
  if (SupportsPresenceApi(descriptor_)) {
    printer->Print(variables_,
      "/// <summary>Gets whether the \"$descriptor_name$\" field is set</summary>\n");
    AddPublicMemberAttributes(printer);
    printer->Print(
      variables_,
      "$access_level$ bool Has$property_name$ {\n"
      "  get { return $has_property_check_internal$; }\n"
      "}");
  }

  // The "ClearFoo" method, where required.
  if (SupportsPresenceApi(descriptor_)) {
    printer->Print(variables_,
      "/// <summary>Clears the value of the \"$descriptor_name$\" field</summary>\n");
    AddPublicMemberAttributes(printer);
    printer->Print(
      variables_,
      "private void Clear$property_name$() {\n");
    if (IsNullable(descriptor_)) {
      printer->Print(variables_, "  $name$_ = null;\n");
    } else {
      printer->Print(variables_, "  $clear_has_field$;\n");
    }
    printer->Print("}\n");
  }
}

void PrimitiveFieldGenerator::GenerateMergingCode(io::Printer* printer) {
  if (EmbedBarePublicField(*descriptor_)) {
      printer->Print(
    variables_,
    "if ($other_has_property_check$) {\n"
    "  $property_name$ = other.$property_name$;\n"
    "}\n");
    return;
  }
  printer->Print(
    variables_,
    "if ($other_has_property_check$) {\n"
    "  $property_name$_Internal = other.$property_name$;\n"
    "}\n");
}

void PrimitiveFieldGenerator::GenerateParsingCode(io::Printer* printer) {
  if (EmbedBarePublicField(*descriptor_)) {
     printer->Print(
    variables_,
    "$property_name$ = input.Read$capitalized_type_name$();\n"); 
    return;
  }

  // Note: invoke the property setter rather than writing straight to the field,
  // so that we can normalize "null to empty" for strings and bytes.
  printer->Print(
    variables_,
    "$property_name$_Internal = input.Read$capitalized_type_name$();\n");
}

void PrimitiveFieldGenerator::GenerateSerializationCode(io::Printer* printer) {
  printer->Print(
    variables_,
    "if ($has_property_check$) {\n"
    "  output.WriteRawTag($tag_bytes$);\n"
    "  output.Write$capitalized_type_name$($property_name_existing$);\n"
    "}\n");
} 

void PrimitiveFieldGenerator::GenerateSerializedSizeCode(io::Printer* printer) {
  printer->Print(
    variables_,
    "if ($has_property_check$) {\n");
  printer->Indent();
  int fixedSize = GetFixedSize(descriptor_->type());
  if (fixedSize == -1) {
    printer->Print(
      variables_,
      "size += $tag_size$ + pb::CodedOutputStream.Compute$capitalized_type_name$Size($property_name_existing$);\n");
  } else {
    printer->Print(
      "size += $tag_size$ + $fixed_size$;\n",
      "fixed_size", absl::StrCat(fixedSize),
      "tag_size", variables_["tag_size"]);
  }
  printer->Outdent();
  printer->Print("}\n");
}

void PrimitiveFieldGenerator::WriteHash(io::Printer* printer) {
  const char *text = 
      "if ($has_property_check$) hash ^= $property_name_existing$.GetHashCode();\n";
  if (descriptor_->type() == FieldDescriptor::TYPE_FLOAT) {
    text = "if ($has_property_check$) hash ^= pbc::ProtobufEqualityComparers.BitwiseSingleEqualityComparer.GetHashCode($property_name_existing$);\n";
  } else if (descriptor_->type() == FieldDescriptor::TYPE_DOUBLE) {
    text = "if ($has_property_check$) hash ^= pbc::ProtobufEqualityComparers.BitwiseDoubleEqualityComparer.GetHashCode($property_name_existing$);\n";
  }
	printer->Print(variables_, text);
}
void PrimitiveFieldGenerator::WriteEquals(io::Printer* printer) {
  const char *text = "if ($property_name$ != other.$property_name$) return false;\n";
  if (descriptor_->type() == FieldDescriptor::TYPE_FLOAT) {
    if (SupportsPresenceApi(descriptor_)) {
      text = "if (!pbc::ProtobufEqualityComparers.BitwiseNullableSingleEqualityComparer.Equals($property_name$, other.$property_name$)) return false;\n";
    } else {
      text = "if (!pbc::ProtobufEqualityComparers.BitwiseSingleEqualityComparer.Equals($property_name$, other.$property_name$)) return false;\n";
    }
  } else if (descriptor_->type() == FieldDescriptor::TYPE_DOUBLE) {
    if (SupportsPresenceApi(descriptor_)) {
      text = "if (!pbc::ProtobufEqualityComparers.BitwiseNullableDoubleEqualityComparer.Equals($property_name$, other.$property_name$)) return false;\n";
    } else {
      text = "if (!pbc::ProtobufEqualityComparers.BitwiseDoubleEqualityComparer.Equals($property_name$, other.$property_name$)) return false;\n";
    }
  }
  printer->Print(variables_, text);
}
void PrimitiveFieldGenerator::WriteToString(io::Printer* printer) {
  printer->Print(
    variables_,
    "PrintField(\"$descriptor_name$\", $has_property_check$, $property_name$, writer);\n");
}

void PrimitiveFieldGenerator::GenerateCloningCode(io::Printer* printer) {
  if (EmbedBarePublicField(*descriptor_)) {
    printer->Print(
        variables_,
        "$property_name$ = other.$property_name$;\n");
      return;
  }
  printer->Print(variables_,
    "$property_name$_Internal = other.$property_name$;\n");
}

void PrimitiveFieldGenerator::GenerateCodecCode(io::Printer* printer) {
  printer->Print(
    variables_,
    "pb::FieldCodec.For$capitalized_type_name$($tag$, $default_value$)");
}

void PrimitiveFieldGenerator::GenerateExtensionCode(io::Printer* printer) {
  WritePropertyDocComment(printer, options(), descriptor_);
  AddDeprecatedFlag(printer);
  printer->Print(
    variables_,
    "$access_level$ static readonly pb::Extension<$extended_type$, $type_name$> $property_name$ =\n"
    "  new pb::Extension<$extended_type$, $type_name$>($number$, ");
  GenerateCodecCode(printer);
  printer->Print(");\n");
}

void PrimitiveFieldGenerator:: GenerateStructConstructorCode(io::Printer *printer) {
  if (EmbedBarePublicField(*descriptor_)) {
      printer->Print(
    variables_,
    "  $property_name$ = default;\n");
    return;
  }
  
  printer->Print(
    variables_,
    "  $name$_ = default;\n");
}

PrimitiveOneofFieldGenerator::PrimitiveOneofFieldGenerator(
    const FieldDescriptor* descriptor, int presenceIndex, const Options *options)
    : PrimitiveFieldGenerator(descriptor, presenceIndex, options) {
  SetCommonOneofFieldVariables(&variables_);
}

PrimitiveOneofFieldGenerator::~PrimitiveOneofFieldGenerator() {
}

void PrimitiveOneofFieldGenerator::GenerateMembers(io::Printer* printer) {
  GenerateOneofCaseMembers(printer);
}

void PrimitiveOneofFieldGenerator::GenerateMergingCode(io::Printer* printer) {
  printer->Print(variables_, "$property_name$_Internal = other.$property_name$;\n");
}

void PrimitiveOneofFieldGenerator::WriteToString(io::Printer* printer) {
  printer->Print(variables_,
    "PrintField(\"$descriptor_name$\", $has_property_check$, $property_name$, writer);\n");
}

void PrimitiveOneofFieldGenerator::GenerateParsingCode(io::Printer* printer) {
    printer->Print(
      variables_,
      "$property_name$_Internal = input.Read$capitalized_type_name$();\n");
}

void PrimitiveOneofFieldGenerator::GenerateCloningCode(io::Printer* printer) {
  printer->Print(variables_,
    "$property_name$_Internal = other.$property_name$;\n");
}

void PrimitiveOneofFieldGenerator:: GenerateStructConstructorCode(io::Printer *printer) {
  printer->Print(variables_, "$name$_ = default;\n");
}

}  // namespace csharp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
