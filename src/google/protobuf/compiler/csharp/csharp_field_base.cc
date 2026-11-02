// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "google/protobuf/compiler/csharp/csharp_field_base.h"

#include <cmath>
#include <limits>
#include <sstream>
#include <string>

#include "google/protobuf/compiler/code_generator.h"
#include "absl/log/absl_log.h"
#include "absl/strings/match.h"
#include "google/protobuf/compiler/csharp/csharp_doc_comment.h"
#include "google/protobuf/compiler/csharp/csharp_helpers.h"
#include "google/protobuf/compiler/csharp/names.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/printer.h"
#include "google/protobuf/wire_format.h"
#include "google/protobuf/descriptor_utils.h"

// Must be last.
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {
namespace compiler {
namespace csharp {

namespace {

// Whether the case's own C# type is a non-nullable value type, so that its property's `T?` has to be unwrapped.
// Wrappers of value types are excluded because their C# type is already `int?`.
bool OneofCaseIsNonNullableValueType(const FieldDescriptor& field) {
  if (field.cpp_type() == FieldDescriptor::CPPTYPE_STRING) {
    return false;
  }
  if (field.cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE) {
    return true;
  }
  return !IsWrapperType(&field) && MessageFieldIsValueType(field);
}

}  // namespace

void FieldGeneratorBase::SetCommonFieldVariables(
    absl::flat_hash_map<absl::string_view, std::string>* variables) {
  // Note: this will be valid even though the tag emitted for packed and unpacked versions of
  // repeated fields varies by wire format. The wire format is encoded in the bottom 3 bits, which
  // never effects the tag size.
  int tag_size = internal::WireFormat::TagSize(descriptor_->number(), descriptor_->type());
  int part_tag_size = tag_size;
  if (descriptor_->type() == FieldDescriptor::TYPE_GROUP) {
    part_tag_size /= 2;
  }
  uint tag = internal::WireFormat::MakeTag(descriptor_);
  uint8_t tag_array[5];
  io::CodedOutputStream::WriteTagToArray(tag, tag_array);
  std::string tag_bytes = absl::StrCat(tag_array[0]);
  for (int i = 1; i < part_tag_size; i++) {
    absl::StrAppend(&tag_bytes, ", ", tag_array[i]);
  }

  (*variables)["tag"] = absl::StrCat(tag);
  (*variables)["tag_size"] = absl::StrCat(tag_size);
  (*variables)["tag_bytes"] = tag_bytes;

  if (descriptor_->type() == FieldDescriptor::Type::TYPE_GROUP) {
    tag = internal::WireFormatLite::MakeTag(
        descriptor_->number(),
        internal::WireFormatLite::WIRETYPE_END_GROUP);
    io::CodedOutputStream::WriteTagToArray(tag, tag_array);
    tag_bytes = absl::StrCat(tag_array[0]);
    for (int i = 1; i < part_tag_size; i++) {
      absl::StrAppend(&tag_bytes, ", ", tag_array[i]);
    }

    variables_["end_tag"] = absl::StrCat(tag);
    variables_["end_tag_bytes"] = tag_bytes;
  }

  (*variables)["access_level"] = "public";

  (*variables)["property_name"] = property_name();
  (*variables)["type_name"] = type_name();
  (*variables)["extended_type"] = GetClassName(descriptor_->containing_type());
  (*variables)["name"] = name();
  (*variables)["descriptor_name"] = descriptor_->name();
  (*variables)["default_value"] = default_value();
  (*variables)["capitalized_type_name"] = capitalized_type_name();
  (*variables)["number"] = number();
  if (has_default_value() && !SupportsPresenceApi(descriptor_)) {
    variables->insert({"name_def_message",
                       absl::StrCat((*variables)["name"],
                                    "_ = ", (*variables)["default_value"])});
  } else {
    variables->insert(
        {"name_def_message", absl::StrCat((*variables)["name"], "_")});
  }
  if (SupportsPresenceApi(descriptor_)) {
    
    if (presenceIndex_ != -1) {
        const int hasBitsNumber = presenceIndex_ / 32;
        const int hasBitsMask = 1 << (presenceIndex_ % 32);
        (*variables)["has_field_check"] = absl::StrCat(
            "(_hasBits", hasBitsNumber, " & ", hasBitsMask, ") != 0");
        (*variables)["set_has_field"] =
            absl::StrCat("_hasBits", hasBitsNumber, " |= ", hasBitsMask);
        (*variables)["clear_has_field"] =
            absl::StrCat("_hasBits", hasBitsNumber, " &= ~", hasBitsMask);
    }
  }

  bool filter = false;//descriptor_->name() == "direction" && descriptor_->containing_type()->name() == "DevicePosition";
  if (EmbedBarePublicField(*descriptor_) && !descriptor_->real_containing_oneof()) {
    if (filter) {
        ABSL_LOG(ERROR) << "x";
    }
    variables->insert({{ "reading_member", (*variables)["property_name"] }});
    variables->insert({{ "writing_member", (*variables)["property_name"] }} );
  } else if (descriptor_->real_containing_oneof()) {
    if (filter) {
        ABSL_LOG(ERROR) << "x";
    }
    variables->insert({{ "reading_member", (*variables)["property_name"] }});
    variables->insert({{"writing_member", (*variables)["property_name"] + "_Internal" }});
  } else if (EmbedReadOnlyRefField(*descriptor_)) {
    if (filter) {
        ABSL_LOG(ERROR) << "x";
    }
    variables->insert({{ "reading_member", (*variables)["property_name"] }});
    variables->insert({{ "writing_member", (*variables)["name"] + '_' }});
  } else {
        if (filter) {
        ABSL_LOG(ERROR) << "x";
    }
    variables->insert({{ "reading_member", (*variables)["property_name"] }});
    variables->insert({{ "writing_member", (*variables)["property_name"] + "_Internal" }}); 
  }
  if (filter) {
    ABSL_LOG(ERROR) << "Field: " << name() << " Presence: " << SupportsPresenceApi(descriptor_)
                    << " Nullable: " << IsNullable(descriptor_) << " is optional: " << descriptor_->is_optional()
                    << "descriptor: " << descriptor_->DebugString();
  }

  if (SupportsPresenceApi(descriptor_) || IsNullable(descriptor_) || descriptor_->real_containing_oneof()) {
    variables->insert({"type_at_rest",
                       absl::StrCat((*variables)["type_name"], "?")});
    variables->insert({"has_property_check",
                       absl::StrCat((*variables)["reading_member"], " is ", type_name(), " ", (*variables)["property_name"], "Value")});
    variables->insert(
        {"other_has_property_check",
         absl::StrCat("other.", (*variables)["reading_member"], " is ", type_name(), " ", (*variables)["property_name"], "OtherValue")});
    variables->insert({"has_not_property_check",
                       absl::StrCat("!(", (*variables)["has_property_check"], ")")});
    variables->insert(
        {"other_has_not_property_check",
         absl::StrCat("!", (*variables)["other_has_property_check"])});

    variables->insert({"property_name_existing", absl::StrCat((*variables)["property_name"], "Value")});
    variables->insert({"other_property_name_existing", absl::StrCat((*variables)["property_name"], "OtherValue")});

    if (IsNullable(descriptor_))
    {
        (*variables)["has_property_check_internal"] = absl::StrCat(this->variables_["reading_member"], " != null");
    }
    else {
      if (EmbedReadOnlyRefField(*descriptor_)) {
        (*variables)["has_property_check_internal"] = absl::StrCat("!", (*variables)["reading_member"], ".Equals(default)");
      } else {
        (*variables)["has_property_check_internal"] = (*variables)["has_field_check"];
      }
    }
  }
  else {
      variables->insert({"type_at_rest", (*variables)["type_name"]});
      variables->insert({ "has_property_check",
                        absl::StrCat("!", (*variables)["reading_member"],
                                     ".Equals(", (*variables)["default_value"], ")") });
      variables->insert({ "other_has_property_check",
                         absl::StrCat("!other.", (*variables)["reading_member"],
                                      ".Equals(", (*variables)["default_value"], ")") });
      variables->insert({ "property_name_existing", absl::StrCat((*variables)["reading_member"]) });
      variables->insert({ "other_property_name_existing", absl::StrCat("other.", (*variables)["reading_member"]) });


      variables->insert({ "has_property_check_internal", (*variables)["has_property_check"] });
  }
}

void FieldGeneratorBase::SetCommonOneofFieldVariables(
    absl::flat_hash_map<absl::string_view, std::string>* variables) {
  (*variables)["oneof_name"] = oneof_name();
  (*variables)["has_property_check_internal"] =
  absl::StrCat(oneof_name() + "Case_ == " + oneof_property_name() +
      "OneofCase." + oneof_case_name());

  variables->insert({ "property_name_existing", absl::StrCat((*variables)["property_name"], "Value") });
  variables->insert({ "other_property_name_existing", absl::StrCat((*variables)["property_name"], "OtherValue") });

  (*variables)["oneof_case_name"] = oneof_case_name();
  (*variables)["oneof_property_name"] = oneof_property_name();

  const std::string type = type_name();
  const std::string nullable_type = absl::EndsWith(type, "?") ? type : absl::StrCat(type, "?");
  const bool non_nullable_value_type = OneofCaseIsNonNullableValueType(*descriptor_);
  (*variables)["type_at_rest"] = nullable_type;
  (*variables)["oneof_case_field_type"] = non_nullable_value_type ? type : nullable_type;
  (*variables)["oneof_case_written_value"] = non_nullable_value_type ? "value.Value" : "value";

  // A record shares one object field between its cases, so its value-type cases box. A struct exists to avoid heap
  // allocation, so each of its cases gets a field of its own type.
  const std::string case_field = absl::StrCat(name(), "_");
  const std::string shared_field = absl::StrCat(oneof_name(), "_");
  const bool owned_by_record = FieldInsideReferenceContainer(*descriptor_);
  (*variables)["oneof_case_storage"] = owned_by_record ? shared_field : case_field;
  (*variables)["oneof_case_read"] = owned_by_record ? absl::StrCat("(", type, ") ", shared_field, "!") : case_field;
  (*variables)["property_setter"] = owned_by_record ? "init" : "set";
}

void FieldGeneratorBase::GenerateOneofCaseMembers(io::Printer* printer) {
  if (!FieldInsideReferenceContainer(*descriptor_)) {
    printer->Print(
      variables_,
      "private $oneof_case_field_type$ $name$_;\n"
      "public static $type_at_rest$ __$property_name$($extended_type$ message) { return message.$property_name$; }\n"
      "public static bool __$property_name$_has_value($extended_type$ message) { return message.$oneof_name$Case_ == $oneof_property_name$OneofCase.$oneof_case_name$; }\n");
  }
  WritePropertyDocComment(printer, options(), descriptor_);
  AddPublicMemberAttributes(printer);
  // The clear comes first so that in a struct the only non-default case field is the active one, and a stale reference
  // in a case field that was switched away from cannot keep its target alive.
  printer->Print(
    variables_,
    "$access_level$ $type_at_rest$ $property_name$ {\n"
    "  get { return $property_name$_Internal; }\n"
    "  $property_setter$ { $property_name$_Internal = value; }\n"
    "}\n"
    "private $type_at_rest$ $property_name$_Internal {\n"
    "  get { return $has_property_check_internal$ ? $oneof_case_read$ : ($type_at_rest$) null; }\n"
    "  set {\n"
    "    Clear$oneof_property_name$();\n"
    "    if (value == null) { return; }\n"
    "    $oneof_case_storage$ = $oneof_case_written_value$;\n"
    "    $oneof_name$Case_ = $oneof_property_name$OneofCase.$oneof_case_name$;\n"
    "  }\n"
    "}\n"
    "/// <summary>Gets whether the \"$descriptor_name$\" field is set</summary>\n");
  AddPublicMemberAttributes(printer);
  printer->Print(
    variables_,
    "$access_level$ bool Has$property_name$ {\n"
    "  get { return $oneof_name$Case_ == $oneof_property_name$OneofCase.$oneof_case_name$; }\n"
    "}\n"
    "/// <summary> Clears the value of the oneof if it's currently set to \"$descriptor_name$\" </summary>\n");
  AddPublicMemberAttributes(printer);
  printer->Print(
    variables_,
    "private void Clear$property_name$() {\n"
    "  if ($oneof_name$Case_ == $oneof_property_name$OneofCase.$oneof_case_name$) {\n"
    "    Clear$oneof_property_name$();\n"
    "  }\n"
    "}\n");
}

FieldGeneratorBase::FieldGeneratorBase(const FieldDescriptor* descriptor,
                                       int presenceIndex, const Options* options)
    : SourceGeneratorBase(options),
      descriptor_(descriptor),
      presenceIndex_(presenceIndex) {
  SetCommonFieldVariables(&variables_);
}

FieldGeneratorBase::~FieldGeneratorBase() {
}

void FieldGeneratorBase::GenerateFreezingCode(io::Printer* printer) {
  // No-op: only message fields and repeated fields need
  // special handling for freezing, so default to not generating any code.
}

void FieldGeneratorBase::GenerateParseLoopPrologue(io::Printer* printer) {
  // No-op: most fields keep nothing between their occurrences on the wire.
}

void FieldGeneratorBase::GenerateParseLoopEpilogue(io::Printer* printer) {
  // No-op: most fields keep nothing between their occurrences on the wire.
}

void FieldGeneratorBase::GenerateCodecCode(io::Printer* printer) {
    // No-op: expect this to be overridden by appropriate types.
    // Could fail if we get called here though...
}

void FieldGeneratorBase::GenerateExtensionCode(io::Printer* printer) {
  // No-op: only message fields, enum fields, primitives, 
  // and repeated fields need this default is to not generate any code
}

void FieldGeneratorBase::GenerateParsingCode(io::Printer* printer, bool use_parse_context) {
  // for some field types the value of "use_parse_context" doesn't matter,
  // so we fallback to the default implementation.
  GenerateParsingCode(printer);
}

void FieldGeneratorBase::GenerateSerializationCode(io::Printer* printer, bool use_write_context) {
  // for some field types the value of "use_write_context" doesn't matter,
  // so we fallback to the default implementation.
  GenerateSerializationCode(printer);
}

void FieldGeneratorBase::AddDeprecatedFlag(io::Printer* printer) {
  if (descriptor_->options().deprecated()) {
    printer->Print("[global::System.ObsoleteAttribute]\n");
  } else if (descriptor_->type() == FieldDescriptor::TYPE_MESSAGE &&
           descriptor_->message_type()->options().deprecated()) {
    printer->Print("[global::System.ObsoleteAttribute]\n");
  }
}

void FieldGeneratorBase::AddPublicMemberAttributes(io::Printer* printer) {
  AddDeprecatedFlag(printer);
  WriteGeneratedCodeAttributes(printer);
}

std::string FieldGeneratorBase::oneof_case_name() {
  return GetOneofCaseName(descriptor_);
}

std::string FieldGeneratorBase::oneof_property_name() {
  return UnderscoresToCamelCase(descriptor_->containing_oneof()->name(), true);
}

std::string FieldGeneratorBase::oneof_name() {
  return UnderscoresToCamelCase(descriptor_->containing_oneof()->name(), false);
}

std::string FieldGeneratorBase::property_name() {
  return GetPropertyName(descriptor_);
}

std::string FieldGeneratorBase::name() {
  return UnderscoresToCamelCase(GetFieldName(descriptor_), false);
}

std::string FieldGeneratorBase::type_name() {
  return type_name(descriptor_);
}

std::string FieldGeneratorBase::type_name(const FieldDescriptor* descriptor) {
  switch (descriptor->type()) {
    case FieldDescriptor::TYPE_ENUM:
      return GetClassName(descriptor->enum_type());
    case FieldDescriptor::TYPE_MESSAGE:
    case FieldDescriptor::TYPE_GROUP:
      if (IsWrapperType(descriptor)) {
        const FieldDescriptor* wrapped_field =
            descriptor->message_type()->field(0);
        std::string wrapped_field_type_name = type_name(wrapped_field);
        // String and ByteString go to the same type; other wrapped types
        // go to the nullable equivalent.
        if (wrapped_field->type() == FieldDescriptor::TYPE_STRING ||
            wrapped_field->type() == FieldDescriptor::TYPE_BYTES) {
          return wrapped_field_type_name;
        } else {
          return absl::StrCat(wrapped_field_type_name, "?");
        }
      }
      return GetClassName(descriptor->message_type());
    case FieldDescriptor::TYPE_DOUBLE:
      return "double";
    case FieldDescriptor::TYPE_FLOAT:
      return "float";
    case FieldDescriptor::TYPE_INT64:
      return "long";
    case FieldDescriptor::TYPE_UINT64:
      return "ulong";
    case FieldDescriptor::TYPE_INT32:
      return "int";
    case FieldDescriptor::TYPE_FIXED64:
      return "ulong";
    case FieldDescriptor::TYPE_FIXED32:
      return "uint";
    case FieldDescriptor::TYPE_BOOL:
      return "bool";
    case FieldDescriptor::TYPE_STRING:
      return "string";
    case FieldDescriptor::TYPE_BYTES:
      return "pb::ByteString";
    case FieldDescriptor::TYPE_UINT32:
      return "uint";
    case FieldDescriptor::TYPE_SFIXED32:
      return "int";
    case FieldDescriptor::TYPE_SFIXED64:
      return "long";
    case FieldDescriptor::TYPE_SINT32:
      return "int";
    case FieldDescriptor::TYPE_SINT64:
      return "long";
    default:
      ABSL_LOG(FATAL) << "Unknown field type.";
      return "";
  }
}

bool FieldGeneratorBase::has_default_value() {
  switch (descriptor_->type()) {
    case FieldDescriptor::TYPE_ENUM:
    case FieldDescriptor::TYPE_MESSAGE:
    case FieldDescriptor::TYPE_GROUP:
      return true;
    case FieldDescriptor::TYPE_DOUBLE:
      return descriptor_->default_value_double() != 0.0;
    case FieldDescriptor::TYPE_FLOAT:
      return descriptor_->default_value_float() != 0.0;
    case FieldDescriptor::TYPE_INT64:
      return descriptor_->default_value_int64() != 0L;
    case FieldDescriptor::TYPE_UINT64:
      return descriptor_->default_value_uint64() != 0L;
    case FieldDescriptor::TYPE_INT32:
      return descriptor_->default_value_int32() != 0;
    case FieldDescriptor::TYPE_FIXED64:
      return descriptor_->default_value_uint64() != 0L;
    case FieldDescriptor::TYPE_FIXED32:
      return descriptor_->default_value_uint32() != 0;
    case FieldDescriptor::TYPE_BOOL:
      return descriptor_->default_value_bool();
    case FieldDescriptor::TYPE_STRING:
      return true;
    case FieldDescriptor::TYPE_BYTES:
      return true;
    case FieldDescriptor::TYPE_UINT32:
      return descriptor_->default_value_uint32() != 0;
    case FieldDescriptor::TYPE_SFIXED32:
      return descriptor_->default_value_int32() != 0;
    case FieldDescriptor::TYPE_SFIXED64:
      return descriptor_->default_value_int64() != 0L;
    case FieldDescriptor::TYPE_SINT32:
      return descriptor_->default_value_int32() != 0;
    case FieldDescriptor::TYPE_SINT64:
      return descriptor_->default_value_int64() != 0L;
    default:
      ABSL_LOG(FATAL) << "Unknown field type.";
      return true;
  }
}

bool AllPrintableAscii(absl::string_view text) {
  for(int i = 0; i < text.size(); i++) {
    if (text[i] < 0x20 || text[i] > 0x7e) {
      return false;
    }
  }
  return true;
}

std::string FieldGeneratorBase::GetStringDefaultValueInternal(const FieldDescriptor* descriptor) {
    if (descriptor->default_value_string().empty())
        return "\"\"";
    return absl::StrCat(
        "global::System.Text.Encoding.UTF8.GetString(global::System."
        "Convert.FromBase64String(\"",
        StringToBase64(descriptor->default_value_string()), "\"), 0, ",
        descriptor->default_value_string().length(), ")");
}

std::string FieldGeneratorBase::GetBytesDefaultValueInternal(const FieldDescriptor* descriptor) {
    if (descriptor->default_value_string().empty())
        return "pb::ByteString.Empty";
    return absl::StrCat("pb::ByteString.FromBase64(\"",
                        StringToBase64(descriptor->default_value_string()),
                        "\")");
}

std::string FieldGeneratorBase::default_value() {
    return default_value(descriptor_);
}

std::string FieldGeneratorBase::default_value(const FieldDescriptor* descriptor) {
  switch (descriptor->type()) {
    case FieldDescriptor::TYPE_ENUM:
      return absl::StrCat(
          GetClassName(descriptor->default_value_enum()->type()), ".",
          GetEnumValueName(descriptor->default_value_enum()->type()->name(),
                           descriptor->default_value_enum()->name()));
    case FieldDescriptor::TYPE_MESSAGE:
    case FieldDescriptor::TYPE_GROUP:
      if (IsWrapperType(descriptor)) {
        const FieldDescriptor* wrapped_field = descriptor->message_type()->field(0);
        return default_value(wrapped_field);
      } else {
        return "default";
      }
    case FieldDescriptor::TYPE_DOUBLE: {
      double value = descriptor->default_value_double();
      if (value == std::numeric_limits<double>::infinity()) {
        return "double.PositiveInfinity";
      } else if (value == -std::numeric_limits<double>::infinity()) {
        return "double.NegativeInfinity";
      } else if (std::isnan(value)) {
        return "double.NaN";
      }
      return absl::StrCat(value, "D");
    }
    case FieldDescriptor::TYPE_FLOAT: {
      float value = descriptor->default_value_float();
      if (value == std::numeric_limits<float>::infinity()) {
        return "float.PositiveInfinity";
      } else if (value == -std::numeric_limits<float>::infinity()) {
        return "float.NegativeInfinity";
      } else if (std::isnan(value)) {
        return "float.NaN";
      }
      return absl::StrCat(value, "F");
    }
    case FieldDescriptor::TYPE_INT64:
      return absl::StrCat(descriptor->default_value_int64(), "L");
    case FieldDescriptor::TYPE_UINT64:
      return absl::StrCat(descriptor->default_value_uint64(), "UL");
    case FieldDescriptor::TYPE_INT32:
      return absl::StrCat(descriptor->default_value_int32());
    case FieldDescriptor::TYPE_FIXED64:
      return absl::StrCat(descriptor->default_value_uint64(), "UL");
    case FieldDescriptor::TYPE_FIXED32:
      return absl::StrCat(descriptor->default_value_uint32());
    case FieldDescriptor::TYPE_BOOL:
      if (descriptor->default_value_bool()) {
        return "true";
      } else {
        return "false";
      }
    case FieldDescriptor::TYPE_STRING:
      return GetStringDefaultValueInternal(descriptor);
    case FieldDescriptor::TYPE_BYTES:
      return GetBytesDefaultValueInternal(descriptor);
    case FieldDescriptor::TYPE_UINT32:
      return absl::StrCat(descriptor->default_value_uint32());
    case FieldDescriptor::TYPE_SFIXED32:
      return absl::StrCat(descriptor->default_value_int32());
    case FieldDescriptor::TYPE_SFIXED64:
      return absl::StrCat(descriptor->default_value_int64(), "L");
    case FieldDescriptor::TYPE_SINT32:
      return absl::StrCat(descriptor->default_value_int32());
    case FieldDescriptor::TYPE_SINT64:
      return absl::StrCat(descriptor->default_value_int64(), "L");
    default:
      ABSL_LOG(FATAL) << "Unknown field type.";
      return "";
  }
}

std::string FieldGeneratorBase::number() {
  return absl::StrCat(descriptor_->number());
}

std::string FieldGeneratorBase::capitalized_type_name() {
  switch (descriptor_->type()) {
    case FieldDescriptor::TYPE_ENUM:
      return "Enum";
    case FieldDescriptor::TYPE_MESSAGE:
      return "Message";
    case FieldDescriptor::TYPE_GROUP:
      return "Group";
    case FieldDescriptor::TYPE_DOUBLE:
      return "Double";
    case FieldDescriptor::TYPE_FLOAT:
      return "Float";
    case FieldDescriptor::TYPE_INT64:
      return "Int64";
    case FieldDescriptor::TYPE_UINT64:
      return "UInt64";
    case FieldDescriptor::TYPE_INT32:
      return "Int32";
    case FieldDescriptor::TYPE_FIXED64:
      return "Fixed64";
    case FieldDescriptor::TYPE_FIXED32:
      return "Fixed32";
    case FieldDescriptor::TYPE_BOOL:
      return "Bool";
    case FieldDescriptor::TYPE_STRING:
      return "String";
    case FieldDescriptor::TYPE_BYTES:
      return "Bytes";
    case FieldDescriptor::TYPE_UINT32:
      return "UInt32";
    case FieldDescriptor::TYPE_SFIXED32:
      return "SFixed32";
    case FieldDescriptor::TYPE_SFIXED64:
      return "SFixed64";
    case FieldDescriptor::TYPE_SINT32:
      return "SInt32";
    case FieldDescriptor::TYPE_SINT64:
      return "SInt64";
    default:
      ABSL_LOG(FATAL) << "Unknown field type.";
      return "";
  }
}

}  // namespace csharp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#include "google/protobuf/port_undef.inc"
