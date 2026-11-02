using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using Google.Protobuf.TestProtos.Immutability;
using FieldMask = Google.Protobuf.WellKnownTypes.FieldMask;
using NUnit.Framework;
using Proto2 = Google.Protobuf.TestProtos.Proto2;

namespace Google.Protobuf
{
    /// <summary>
    /// A generated record is immutable to its users and a generated struct is not. Each writable surface is checked on
    /// both owners, so the struct rows prove that the checks can see a writable member at all.
    /// </summary>
    public class ImmutabilityTest
    {
        private static readonly string[] SharedPropertyNames =
        {
            "PrimitiveField", "StringField", "BytesField", "EnumField", "MessageField", "StructField",
            "OptionalPrimitiveField", "OptionalStringField", "OptionalBytesField", "OptionalEnumField",
            "OptionalMessageField",
            "IntWrapperField", "StringWrapperField", "BytesWrapperField",
            "RepeatedPrimitiveField", "RepeatedStringField", "RepeatedEnumField", "RepeatedMessageField",
            "MapField", "MapMessageField",
            "PrimitiveCase", "StringCase", "BytesCase", "EnumCase", "MessageCase", "StructCase",
            "IntWrapperCase", "StringWrapperCase",
        };

        private static readonly string[] RecordOnlyPropertyNames =
        {
            "RefFieldSetter",
        };

        private static IEnumerable<TestCaseData> FieldKindRows()
        {
            foreach (string memberName in SharedPropertyNames.Concat(second: RecordOnlyPropertyNames))
            {
                yield return new TestCaseData(typeof(RecordOwner), memberName, false).SetName($"Record_{memberName}_NotWritable");
            }
            foreach (string memberName in SharedPropertyNames)
            {
                yield return new TestCaseData(typeof(StructOwner), memberName, true).SetName($"Struct_{memberName}_Writable");
            }
        }

        [TestCaseSource(nameof(FieldKindRows))]
        public void FieldKind_IsWritableAfterConstruction_OnlyOnStruct(Type ownerType, string memberName, bool expectedWritable)
        {
            MemberInfo member = ownerType.GetMember(name: memberName, bindingAttr: BindingFlags.Instance | BindingFlags.Public).Single();

            Assert.AreEqual(expected: expectedWritable, actual: IsWritableAfterConstruction(member: member));
        }

        // The rows above name the kinds; this catches a member that the rows do not name, such as a new accessor.
        [TestCase(typeof(RecordOwner), false)]
        [TestCase(typeof(RecordLeaf), false)]
        [TestCase(typeof(StructOwner), true)]
        [TestCase(typeof(StructLeaf), true)]
        public void EveryPublicMember_IsWritableAfterConstruction_OnlyOnStruct(Type ownerType, bool expectedAnyWritable)
        {
            bool anyWritable = ownerType
                .GetMembers(bindingAttr: BindingFlags.Instance | BindingFlags.Public)
                .Any(predicate: static member => IsWritableAfterConstruction(member: member));

            Assert.AreEqual(expected: expectedAnyWritable, actual: anyWritable);
        }

        private static IEnumerable<TestCaseData> ClearRows()
        {
            yield return new TestCaseData(typeof(RecordOwner), "ClearChoice", false).SetName("Record_ClearChoice_NotPublic");
            yield return new TestCaseData(typeof(StructOwner), "ClearChoice", true).SetName("Struct_ClearChoice_Public");
            foreach (string fieldName in new[] { "OptionalPrimitiveField", "OptionalStringField", "OptionalBytesField", "OptionalEnumField", "OptionalMessageField", "MessageField", "IntWrapperField", "StringWrapperField", "BytesWrapperField", "PrimitiveCase", "StringCase", "BytesCase", "EnumCase", "MessageCase", "StructCase", "IntWrapperCase", "StringWrapperCase" })
            {
                yield return new TestCaseData(typeof(RecordOwner), "Clear" + fieldName, false).SetName($"Record_Clear{fieldName}_NotPublic");
            }
        }

        [TestCaseSource(nameof(ClearRows))]
        public void ClearMethod_IsPublic_OnlyOnStructOneof(Type ownerType, string methodName, bool expectedPublic)
        {
            MethodInfo clear = ownerType.GetMethod(name: methodName, bindingAttr: BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);

            Assert.IsNotNull(anObject: clear, message: methodName);
            Assert.AreEqual(expected: expectedPublic, actual: clear.IsPublic);
        }

        [TestCase(typeof(RecordOwner), typeof(RecordOwner), true)]
        [TestCase(typeof(RecordOwner), typeof(CodedInputStream), true)]
        [TestCase(typeof(RecordLeaf), typeof(RecordLeaf), true)]
        [TestCase(typeof(RecordLeaf), typeof(CodedInputStream), true)]
        [TestCase(typeof(StructOwner), typeof(CodedInputStream), false)]
        public void MergeFrom_IsObsolete_OnRecords(Type ownerType, Type sourceType, bool expectedObsolete)
        {
            MethodInfo merge = ownerType.GetMethod(name: "MergeFrom", types: new[] { sourceType });

            Assert.AreEqual(expected: expectedObsolete, actual: merge.IsDefined(attributeType: typeof(ObsoleteAttribute), inherit: false));
        }

        // WriteTo only reads the message. Anything else returning nothing can only be there to change it.
        [TestCase(typeof(RecordOwner), false)]
        [TestCase(typeof(RecordLeaf), false)]
        [TestCase(typeof(StructOwner), true)]
        public void PublicMutatorWithoutObsolete_ExistsOnlyOnStruct(Type ownerType, bool expectedAny)
        {
            bool any = ownerType
                .GetMethods(bindingAttr: BindingFlags.Instance | BindingFlags.Public | BindingFlags.DeclaredOnly)
                .Any(predicate: static method =>
                    !method.IsSpecialName
                    && method.ReturnType == typeof(void)
                    && method.Name != "WriteTo"
                    && !method.IsDefined(attributeType: typeof(ObsoleteAttribute), inherit: false));

            Assert.AreEqual(expected: expectedAny, actual: any);
        }

        [TestCase(typeof(IReadOnlyDictionary<string, int>), false)]
        [TestCase(typeof(ImmutableArray<int>), false)]
        [TestCase(typeof(Dictionary<string, int>), true)]
        [TestCase(typeof(List<int>), true)]
        public void HasPublicMutator_TellsMutableCollectionsApart(Type collectionType, bool expectedMutators)
        {
            Assert.AreEqual(expected: expectedMutators, actual: HasPublicMutator(collectionType: collectionType));
        }

        [TestCase("RepeatedPrimitiveField")]
        [TestCase("RepeatedStringField")]
        [TestCase("RepeatedEnumField")]
        [TestCase("RepeatedMessageField")]
        [TestCase("MapField")]
        [TestCase("MapMessageField")]
        public void RecordCollectionProperty_DeclaresNoMutators(string propertyName)
        {
            Type declaredType = typeof(RecordOwner).GetProperty(name: propertyName).PropertyType;

            Assert.IsFalse(condition: HasPublicMutator(collectionType: declaredType));
        }

        // A declared read-only type does not help if the object behind it can be cast back to something writable.
        [TestCase(Origins.Constructed)]
        [TestCase(Origins.Parsed)]
        [TestCase(Origins.DeepClone)]
        [TestCase(Origins.WithCopy)]
        [TestCase(Origins.CopyConstructor)]
        public void RecordMap_RuntimeObject_HasNoPublicMutator(Origins origin)
        {
            RecordOwner owner = Copy(origin: origin, source: Populated());

            Assert.IsFalse(condition: HasPublicMutator(collectionType: owner.MapField.GetType()), message: owner.MapField.GetType().FullName);
            Assert.IsFalse(condition: HasPublicMutator(collectionType: owner.MapMessageField.GetType()), message: owner.MapMessageField.GetType().FullName);
        }

        private static IEnumerable<TestCaseData> WithRows()
        {
            yield return WithRow(name: "Scalar", change: static owner => owner with { PrimitiveField = 99 }, read: static owner => owner.PrimitiveField, expectedOriginal: 7, expectedCopy: 99);
            yield return WithRow(name: "String", change: static owner => owner with { StringField = "changed" }, read: static owner => owner.StringField, expectedOriginal: "text", expectedCopy: "changed");
            yield return WithRow(name: "Optional", change: static owner => owner with { OptionalPrimitiveField = 99 }, read: static owner => owner.OptionalPrimitiveField, expectedOriginal: 8, expectedCopy: 99);
            yield return WithRow(name: "Wrapper", change: static owner => owner with { IntWrapperField = 99 }, read: static owner => owner.IntWrapperField, expectedOriginal: 13, expectedCopy: 99);
            yield return WithRow(name: "WrapperToNull", change: static owner => owner with { IntWrapperField = null }, read: static owner => owner.IntWrapperField, expectedOriginal: 13, expectedCopy: null);
            yield return WithRow(name: "NestedMessage", change: static owner => owner with { MessageField = owner.MessageField with { Number = 99 } }, read: static owner => owner.MessageField.Number, expectedOriginal: 5, expectedCopy: 99);
            yield return WithRow(name: "OneofCaseSwitch", change: static owner => owner with { StringCase = "other case" }, read: static owner => owner.ChoiceCase, expectedOriginal: RecordOwner.ChoiceOneofCase.PrimitiveCase, expectedCopy: RecordOwner.ChoiceOneofCase.StringCase);
            yield return WithRow(name: "OneofOldCaseValue", change: static owner => owner with { StringCase = "other case" }, read: static owner => owner.PrimitiveCase, expectedOriginal: 30, expectedCopy: null);
            yield return WithRow(name: "Repeated", change: static owner => owner with { RepeatedPrimitiveField = owner.RepeatedPrimitiveField.Add(item: 99) }, read: static owner => owner.RepeatedPrimitiveField.Length, expectedOriginal: 2, expectedCopy: 3);
            yield return WithRow(name: "Map", change: static owner => owner with { MapField = new Dictionary<string, int> { ["changed"] = 99 } }, read: static owner => owner.MapField.ContainsKey(key: "changed"), expectedOriginal: false, expectedCopy: true);
            yield return WithRow(name: "UnchangedFieldControl", change: static owner => owner with { PrimitiveField = 99 }, read: static owner => owner.StringField, expectedOriginal: "text", expectedCopy: "text");
        }

        [TestCaseSource(nameof(WithRows))]
        public void With_ChangesTheCopy_AndLeavesTheOriginal(Func<RecordOwner, RecordOwner> change, Func<RecordOwner, object> read, object expectedOriginal, object expectedCopy)
        {
            RecordOwner original = Populated();
            byte[] originalBytes = original.ToByteArray();

            RecordOwner copy = change(arg: original);

            Assert.AreEqual(expected: expectedOriginal, actual: read(arg: original));
            Assert.AreEqual(expected: expectedCopy, actual: read(arg: copy));
            CollectionAssert.AreEqual(expected: originalBytes, actual: original.ToByteArray());
        }

        // Merging is the one thing left that can write into an existing record, and is how a parser fills one, so it
        // is the probe for storage shared between two records.
        private static IEnumerable<TestCaseData> SharedStateRows()
        {
            foreach (Origins origin in new[] { Origins.Parsed, Origins.DeepClone, Origins.WithCopy, Origins.CopyConstructor })
            {
                foreach (Writes write in new[] { Writes.Repeated, Writes.Map, Writes.Nested })
                {
                    yield return new TestCaseData(origin, write).SetName($"{origin}_{write}");
                }
            }
        }

        [TestCaseSource(nameof(SharedStateRows))]
        public void MergingIntoACopy_LeavesTheSource(Origins origin, Writes write)
        {
            RecordOwner source = Copy(origin: Origins.Parsed, source: Populated());
            byte[] sourceBytes = source.ToByteArray();
            RecordOwner copy = Copy(origin: origin, source: source);

            MergeWire(target: copy, write: write);

            CollectionAssert.AreEqual(expected: sourceBytes, actual: source.ToByteArray());
            CollectionAssert.AreNotEqual(expected: sourceBytes, actual: copy.ToByteArray(), message: "control: the merge did write");
        }

        [TestCaseSource(nameof(SharedStateRows))]
        public void MergingIntoTheSource_LeavesACopy(Origins origin, Writes write)
        {
            RecordOwner source = Copy(origin: Origins.Parsed, source: Populated());
            RecordOwner copy = Copy(origin: origin, source: source);
            byte[] copyBytes = copy.ToByteArray();

            MergeWire(target: source, write: write);

            CollectionAssert.AreEqual(expected: copyBytes, actual: copy.ToByteArray());
            CollectionAssert.AreNotEqual(expected: copyBytes, actual: source.ToByteArray(), message: "control: the merge did write");
        }

        // Parsing, cloning and merging all reach the wrapper's storage through its private setter.
        [TestCase("IntWrapperField", 13)]
        [TestCase("StringWrapperField", "wrapped")]
        [TestCase("BytesWrapperField", null)]
        public void RecordWrapperField_KeepsItsValueThroughParseCloneAndMerge(string propertyName, object expected)
        {
            RecordOwner written = new RecordOwner { IntWrapperField = 13, StringWrapperField = "wrapped" };

            RecordOwner parsed = RecordOwner.Parser.ParseFrom(data: written.ToByteArray());
            RecordOwner merged = new RecordOwner();
#pragma warning disable CS0618 // Merging is one of the three paths under test.
            merged.MergeFrom(other: parsed);
#pragma warning restore CS0618

            PropertyInfo property = typeof(RecordOwner).GetProperty(name: propertyName);
            Assert.AreEqual(expected: expected, actual: property.GetValue(obj: parsed));
            Assert.AreEqual(expected: expected, actual: property.GetValue(obj: parsed.DeepClone()));
            Assert.AreEqual(expected: expected, actual: property.GetValue(obj: merged));
        }

        // The struct is parsed as a field of a record, because that path writes the parsed struct back to its owner.
        [TestCase("IntWrapperField", 13)]
        [TestCase("StringWrapperField", "wrapped")]
        [TestCase("BytesWrapperField", null)]
        public void StructWrapperField_KeepsItsValueThroughParseCloneAndMerge(string propertyName, object expected)
        {
            StructOwner written = new StructOwner { IntWrapperField = 13, StringWrapperField = "wrapped" };
            byte[] wire = new StructOwnerHolder { Owner = written }.ToByteArray();

            StructOwner parsed = StructOwnerHolder.Parser.ParseFrom(data: wire).Owner;
            StructOwner merged = new StructOwner();
#pragma warning disable CS0618 // Merging is one of the three paths under test.
            merged.MergeFrom(other: parsed);
#pragma warning restore CS0618

            PropertyInfo property = typeof(StructOwner).GetProperty(name: propertyName);
            Assert.AreEqual(expected: expected, actual: property.GetValue(obj: parsed));
            Assert.AreEqual(expected: expected, actual: property.GetValue(obj: parsed.DeepClone()));
            Assert.AreEqual(expected: expected, actual: property.GetValue(obj: merged));
        }

        // Reflection on a struct needs the generated static accessors: FieldDescriptor throws without them.
        [TestCase(typeof(RecordOwner), "int_wrapper_field", 13)]
        [TestCase(typeof(RecordOwner), "bytes_wrapper_field", null)]
        [TestCase(typeof(StructOwner), "int_wrapper_field", 13)]
        [TestCase(typeof(StructOwner), "bytes_wrapper_field", null)]
        public void WrapperField_IsReadableThroughTheDescriptor(Type ownerType, string fieldName, object expected)
        {
            IMessage message = ownerType == typeof(RecordOwner)
                ? new RecordOwner { IntWrapperField = 13 }
                : new StructOwner { IntWrapperField = 13 };

            object value = message.Descriptor.FindFieldByName(name: fieldName).Accessor.GetValue(message: message);

            Assert.AreEqual(expected: expected, actual: value);
        }

        // Field 16 is repeated int32 and field 1 is int32: tag 128 1 is one unpacked element, 130 1 starts a packed run.
        [TestCase(new byte[] { 128, 1, 1, 128, 1, 2 }, new[] { 1, 2 }, TestName = "Parse_Repeated_ConsecutiveElements")]
        [TestCase(new byte[] { 128, 1, 1, 8, 7, 128, 1, 2 }, new[] { 1, 2 }, TestName = "Parse_Repeated_SplitByAnotherField")]
        [TestCase(new byte[] { 130, 1, 1, 1, 130, 1, 1, 2 }, new[] { 1, 2 }, TestName = "Parse_Repeated_TwoPackedRuns")]
        [TestCase(new byte[] { 8, 7 }, new int[0], TestName = "Parse_Repeated_Absent")]
        public void Parse_KeepsEveryRunOfARepeatedField(byte[] wire, int[] expected)
        {
            RecordOwner parsed = RecordOwner.Parser.ParseFrom(data: wire);

            CollectionAssert.AreEqual(expected: expected, actual: parsed.RepeatedPrimitiveField);
        }

        // default(StructOwner) never ran a constructor, so its repeated fields hold no collection until one is read.
        [TestCase(new byte[0], new int[0], 0, TestName = "StructRepeated_Empty")]
        [TestCase(new byte[] { 128, 1, 1, 8, 7, 128, 1, 2 }, new[] { 1, 2 }, 7, TestName = "StructRepeated_SplitByAnotherField")]
        public void Struct_RepeatedField_ParsesFromAnUnconstructedValue(byte[] wire, int[] expected, int expectedPackedSize)
        {
            StructOwner parsed = default;
            parsed.MergeFrom(input: new CodedInputStream(buffer: wire));

            CollectionAssert.AreEqual(expected: expected, actual: parsed.RepeatedPrimitiveField);
            Assert.AreEqual(expected: expectedPackedSize, actual: parsed.CalculateSize());
        }

        // The accessor has to store the empty collection, not write a scalar default into the collection's property.
        [TestCase("repeated_primitive_field", 0, TestName = "StructClear_Repeated")]
        [TestCase("primitive_field", 1, TestName = "StructClear_ControlOtherField")]
        public void DescriptorClear_ReachesAStructsRepeatedField(string fieldName, int expectedCount)
        {
            IMessage owner = new StructOwner { RepeatedPrimitiveField = ImmutableArray.Create(item: 1) };

            StructOwner.Descriptor.FindFieldByName(name: fieldName).Accessor.Clear(message: owner);

            Assert.AreEqual(expected: expectedCount, actual: ((StructOwner) owner).RepeatedPrimitiveField.Length);
        }

        [TestCase(null, 0)]
        [TestCase("", 0)]
        [TestCase("text", 6)]
        public void Struct_WithUnassignedStringField_Serializes(string text, int expectedSize)
        {
            StructOwner owner = new StructOwner { StringField = text };

            Assert.AreEqual(expected: expectedSize, actual: owner.CalculateSize());
            Assert.AreEqual(expected: expectedSize, actual: owner.ToByteArray().Length);
            Assert.DoesNotThrow(code: () => owner.GetHashCode());
        }

        private static IEnumerable<TestCaseData> RecordJsonRows()
        {
            yield return new TestCaseData("{}", new RecordOwner()).SetName("Json_Empty");
            yield return new TestCaseData("{\"repeatedPrimitiveField\":[1,2]}", new RecordOwner { RepeatedPrimitiveField = ImmutableArray.Create(item1: 1, item2: 2) }).SetName("Json_RepeatedPrimitive");
            yield return new TestCaseData("{\"repeatedStringField\":[\"a\"]}", new RecordOwner { RepeatedStringField = ImmutableArray.Create(item: "a") }).SetName("Json_RepeatedString");
            yield return new TestCaseData("{\"repeatedEnumField\":[\"SHADE_DARK\"]}", new RecordOwner { RepeatedEnumField = ImmutableArray.Create(item: Shade.Dark) }).SetName("Json_RepeatedEnum");
            yield return new TestCaseData("{\"repeatedMessageField\":[{\"number\":1}]}", new RecordOwner { RepeatedMessageField = ImmutableArray.Create(item: new RecordLeaf { Number = 1 }) }).SetName("Json_RepeatedMessage");
            yield return new TestCaseData("{\"repeatedPrimitiveField\":null}", new RecordOwner()).SetName("Json_RepeatedNull");
            yield return new TestCaseData("{\"mapField\":{\"one\":1}}", new RecordOwner { MapField = new Dictionary<string, int> { ["one"] = 1 } }).SetName("Json_Map");
            yield return new TestCaseData("{\"mapMessageField\":{\"leaf\":{\"number\":1}}}", new RecordOwner { MapMessageField = new Dictionary<string, RecordLeaf> { ["leaf"] = new RecordLeaf { Number = 1 } } }).SetName("Json_MapMessage");
            yield return new TestCaseData("{\"mapField\":null}", new RecordOwner()).SetName("Json_MapNull");
            yield return new TestCaseData("{\"structField\":null}", new RecordOwner()).SetName("Json_StructFieldNull");
            yield return new TestCaseData("{\"structField\":{\"number\":5}}", new RecordOwner { StructField = new StructLeaf { Number = 5 } }).SetName("Json_StructField");
        }

        [TestCaseSource(nameof(RecordJsonRows))]
        public void Json_FillsAnImmutableRecord(string json, RecordOwner expected)
        {
            RecordOwner parsed = JsonParser.Default.Parse<RecordOwner>(json: json);

            Assert.AreEqual(expected: expected, actual: parsed);
        }

        private static IEnumerable<TestCaseData> ReflectionClearRows()
        {
            yield return new TestCaseData("repeated_primitive_field", (Func<RecordOwner, object>) (static owner => owner.RepeatedPrimitiveField.Length), 0).SetName("Clear_Repeated");
            yield return new TestCaseData("map_field", (Func<RecordOwner, object>) (static owner => owner.MapField.Count), 0).SetName("Clear_Map");
            yield return new TestCaseData("struct_field", (Func<RecordOwner, object>) (static owner => owner.StructField.Number), 0).SetName("Clear_StructField");
            yield return new TestCaseData("primitive_field", (Func<RecordOwner, object>) (static owner => owner.StructField.Number), 9).SetName("Clear_ControlLeavesOtherFields");
        }

        [TestCaseSource(nameof(ReflectionClearRows))]
        public void DescriptorClear_ReachesARecordsImmutableFields(string fieldName, Func<RecordOwner, object> read, object expected)
        {
            RecordOwner owner = new RecordOwner
            {
                RepeatedPrimitiveField = ImmutableArray.Create(item: 1),
                MapField = new Dictionary<string, int> { ["one"] = 1 },
                StructField = new StructLeaf { Number = 9 },
            };

            RecordOwner.Descriptor.FindFieldByName(name: fieldName).Accessor.Clear(message: owner);

            Assert.AreEqual(expected: expected, actual: read(arg: owner));
        }

        // Copies of a struct share its parsed map, which a parse into either of them must not write through.
        [TestCase(true, 1, 2)]
        [TestCase(false, 2, 1)]
        public void StructCopies_MergeWire_ChangesOnlyTheTarget(bool mergeIntoCopy, int expectedOriginalCount, int expectedCopyCount)
        {
            byte[] first = new RecordOwner { MapField = new Dictionary<string, int> { ["one"] = 1 } }.ToByteArray();
            byte[] second = new RecordOwner { MapField = new Dictionary<string, int> { ["two"] = 2 } }.ToByteArray();
            StructOwner original = StructOwner.Parser.ParseFrom(data: first);
            StructOwner copy = original;

            if (mergeIntoCopy)
            {
                copy.MergeFrom(input: new CodedInputStream(buffer: second));
            }
            else
            {
                original.MergeFrom(input: new CodedInputStream(buffer: second));
            }

            Assert.AreEqual(expected: expectedOriginalCount, actual: original.MapField.Count);
            Assert.AreEqual(expected: expectedCopyCount, actual: copy.MapField.Count);
        }

        [TestCase("struct_field.number", 5, TestName = "FieldMask_NestedPath")]
        [TestCase("struct_field", 5, TestName = "FieldMask_WholeField")]
        [TestCase("primitive_field", 1, TestName = "FieldMask_ControlOtherField")]
        public void FieldMask_Merge_ReachesAStructFieldOfAStruct(string path, int expectedNumber)
        {
            IMessage destination = new StructOwner { StructField = new StructLeaf { Number = 1 } };
            StructOwner source = new StructOwner { StructField = new StructLeaf { Number = 5 } };

            new FieldMask { Paths = { path } }.Merge(source: source, destination: destination);

            Assert.AreEqual(expected: expectedNumber, actual: ((StructOwner) destination).StructField.Number);
        }

        // SetExtension stores the object it is given, so its caller still holds whatever a merge would write to. The
        // rows with no source value merge nothing, which shows that the other rows' value comes from the merge.
        [TestCase(ExtensionMerges.Message, 2, 2)]
        [TestCase(ExtensionMerges.Wire, 2, 2)]
        [TestCase(ExtensionMerges.Message, null, 1)]
        [TestCase(ExtensionMerges.Wire, null, 1)]
        public void ExtensionMerge_LeavesTheObjectGivenToSetExtension(ExtensionMerges merge, int? sourceValue, int expectedMerged)
        {
            var given = new Proto2.TestAllTypes.Types.NestedMessage { Bb = 1 };
            var target = new Proto2.TestAllExtensions();
            target.SetExtension(extension: Proto2.UnittestExtensions.OptionalNestedMessageExtension, value: given);
            var source = new Proto2.TestAllExtensions();
            if (sourceValue != null)
            {
                source.SetExtension(extension: Proto2.UnittestExtensions.OptionalNestedMessageExtension, value: new Proto2.TestAllTypes.Types.NestedMessage { Bb = sourceValue.Value });
            }

            if (merge == ExtensionMerges.Message)
            {
                target.MergeFrom(other: source);
            }
            else
            {
                target.MergeFrom(data: source.ToByteArray());
            }

            Assert.AreEqual(expected: 1, actual: given.Bb);
            Assert.AreEqual(expected: expectedMerged, actual: target.GetExtension(extension: Proto2.UnittestExtensions.OptionalNestedMessageExtension).Bb);
        }

        // A record can be held by several messages, so a merge into one of them cannot write into the record itself.
        [TestCase("message_field.number", 9, TestName = "FieldMaskShared_NestedPath")]
        [TestCase("message_field", 9, TestName = "FieldMaskShared_WholeField")]
        [TestCase("primitive_field", 1, TestName = "FieldMaskShared_ControlOtherField")]
        public void FieldMask_Merge_LeavesANestedRecordOthersHold(string path, int expectedNumber)
        {
            RecordLeaf shared = new RecordLeaf { Number = 1 };
            RecordOwner destination = new RecordOwner { MessageField = shared };
            RecordOwner source = new RecordOwner { MessageField = new RecordLeaf { Number = 9 } };

            new FieldMask { Paths = { path } }.Merge(source: source, destination: destination);

            Assert.AreEqual(expected: 1, actual: shared.Number);
            Assert.AreEqual(expected: expectedNumber, actual: destination.MessageField.Number);
        }

        // The wire format writes a struct field only when it differs from its default, and JSON has to agree.
        [TestCase(typeof(RecordOwner), 0, "{ }", 0)]
        [TestCase(typeof(RecordOwner), 5, "{ \"structField\": { \"number\": 5 } }", 4)]
        [TestCase(typeof(StructOwner), 0, "{ }", 0)]
        [TestCase(typeof(StructOwner), 5, "{ \"structField\": { \"number\": 5 } }", 4)]
        public void StructField_EqualToItsDefault_IsOmittedFromJsonAndWire(Type ownerType, int number, string expectedJson, int expectedSize)
        {
            StructLeaf leaf = new StructLeaf { Number = number };
            IMessage owner = ownerType == typeof(RecordOwner)
                ? new RecordOwner { StructField = leaf }
                : new StructOwner { StructField = leaf };

            Assert.AreEqual(expected: expectedJson, actual: JsonFormatter.Default.Format(message: owner));
            Assert.AreEqual(expected: expectedSize, actual: owner.CalculateSize());
        }

        public enum Origins
        {
            Constructed,
            Parsed,
            DeepClone,
            WithCopy,
            CopyConstructor,
        }

        public enum ExtensionMerges
        {
            Message,
            Wire,
        }

        public enum Writes
        {
            Repeated,
            Map,
            Nested,
        }

        private static TestCaseData WithRow(string name, Func<RecordOwner, RecordOwner> change, Func<RecordOwner, object> read, object expectedOriginal, object expectedCopy) =>
            new TestCaseData(change, read, expectedOriginal, expectedCopy).SetName($"With_{name}");

        private static RecordOwner Populated() => new RecordOwner
        {
            PrimitiveField = 7,
            StringField = "text",
            BytesField = ByteString.CopyFromUtf8(text: "bytes"),
            EnumField = Shade.Dark,
            MessageField = new RecordLeaf { Number = 5, Items = ImmutableArray.Create(item1: 1, item2: 2), Entries = new Dictionary<string, int> { ["leaf"] = 1 } },
            OptionalPrimitiveField = 8,
            IntWrapperField = 13,
            StringWrapperField = "wrapped",
            RepeatedPrimitiveField = ImmutableArray.Create(item1: 1, item2: 2),
            RepeatedMessageField = ImmutableArray.Create(item: new RecordLeaf { Number = 1 }),
            MapField = new Dictionary<string, int> { ["one"] = 1 },
            MapMessageField = new Dictionary<string, RecordLeaf> { ["leaf"] = new RecordLeaf { Number = 1 } },
            PrimitiveCase = 30,
        };

        private static RecordOwner Copy(Origins origin, RecordOwner source)
        {
#pragma warning disable CS8524 // Origins is closed: its five values are the rows of the tests above.
            return origin switch
#pragma warning restore CS8524
            {
                Origins.Constructed => source,
                Origins.Parsed => RecordOwner.Parser.ParseFrom(data: source.ToByteArray()),
                Origins.DeepClone => source.DeepClone(),
                Origins.WithCopy => source with { },
                Origins.CopyConstructor => new RecordOwner(other: source),
            };
        }

        private static void MergeWire(RecordOwner target, Writes write)
        {
#pragma warning disable CS8524 // Writes is closed: its three values are the rows of SharedStateRows.
            byte[] wire = write switch
#pragma warning restore CS8524
            {
                Writes.Repeated => new RecordOwner { RepeatedPrimitiveField = ImmutableArray.Create(item: 99) }.ToByteArray(),
                Writes.Map => new RecordOwner { MapField = new Dictionary<string, int> { ["merged"] = 99 } }.ToByteArray(),
                Writes.Nested => new RecordOwner { MessageField = new RecordLeaf { Number = 99 } }.ToByteArray(),
            };
#pragma warning disable CS0618 // The test is what the attribute warns about: writing into an existing record.
            target.MergeFrom(input: new CodedInputStream(buffer: wire));
#pragma warning restore CS0618
        }

        private static bool IsWritableAfterConstruction(MemberInfo member)
        {
            if (member is FieldInfo field)
            {
                return !field.IsInitOnly;
            }
            if (member is not PropertyInfo property || property.GetSetMethod() is not MethodInfo setter)
            {
                return false;
            }
            // Compared by name because net462 and netstandard2.0 take the type from a polyfill in the generated assembly.
            return !setter.ReturnParameter.GetRequiredCustomModifiers().Any(
                predicate: static modifier => modifier.FullName == "System.Runtime.CompilerServices.IsExternalInit");
        }

        // An immutable collection's Add returns the new collection, so returning nothing is what marks a mutator.
        private static bool HasPublicMutator(Type collectionType)
        {
            return collectionType.GetMethods(bindingAttr: BindingFlags.Instance | BindingFlags.Public).Any(
                predicate: static method => method.ReturnType == typeof(void) && method.Name is "Add" or "Clear" or "Remove" or "set_Item" or "set_Capacity");
        }
    }
}
