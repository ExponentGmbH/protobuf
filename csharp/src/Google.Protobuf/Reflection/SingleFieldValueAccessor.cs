using System;
using System.Collections;
using System.Diagnostics.CodeAnalysis;
using System.Reflection;

namespace Google.Protobuf.Reflection
{
    internal sealed class SingleFieldValueAccessor : IFieldAccessor
    {
        private readonly Func<IMessage, bool> hasDelegate;
        private readonly Func<IMessage, object> getDelegate;
        private readonly Action<IMessage, object> setDelegate;
        private readonly Action<IMessage> clearDelegate;
        private FieldDescriptor descriptor;

        internal SingleFieldValueAccessor(
            [DynamicallyAccessedMembers(GeneratedClrTypeInfo.MessageAccessibility)]
            Type messageType, MethodInfo getter, MethodInfo hasValue, FieldDescriptor descriptor)
        {
            string name = descriptor.PropertyName;
            PropertyInfo maybeHasProperty = hasValue == null && descriptor.HasPresence
                ? messageType.GetProperty(name: "Has" + name)
                : null;
            this.getDelegate = ReflectionUtil.CreateFuncIMessageObject(getter);
            // A struct field with no presence check is unset when it equals its default, which the wire format also omits.
            bool isStructField = descriptor.FieldType == FieldType.Message && !descriptor.IsRepeated && getter.ReturnType.IsValueType;
            object maybeStructDefault = isStructField ? Activator.CreateInstance(type: getter.ReturnType) : null;
            this.hasDelegate = hasValue != null
                ? ReflectionUtil.CreateFuncIMessageBool(hasValue)
                : maybeHasProperty != null ? message => (bool) maybeHasProperty.GetValue(obj: message)
                : isStructField ? message => !Equals(getDelegate(message), maybeStructDefault)
                : (_) => true;
            this.descriptor = descriptor;

            // ReflectionUtil's delegates cannot bind to a struct's instance members, whose open form takes `ref T`.
            // FieldInfo and PropertyInfo work on the box itself, so a write reaches the caller's boxed message rather
            // than a copy.
            FieldInfo maybeField = messageType.GetField(name);
            PropertyInfo maybeProperty = maybeField != null ? null : messageType.GetProperty(name: name);
            // The `ref readonly` property of a use_ref field cannot be written, so it comes with an XSetter.
            PropertyInfo maybeWritable = maybeProperty != null && !maybeProperty.CanWrite
                ? messageType.GetProperty(name: name + "Setter")
                : maybeProperty;
            if (maybeField == null && maybeWritable == null)
            {
                throw new ArgumentException($"No field or writable property for {name} in {messageType}");
            }
            Action<IMessage, object> write = maybeField != null
                ? (message, value) => maybeField.SetValue(obj: message, value: value)
                : (message, value) => maybeWritable.SetValue(obj: message, value: value);

            // JsonParser hands over enum values as boxed ints, which do not convert to a nullable enum.
            Type storedType = maybeField != null ? maybeField.FieldType : maybeWritable.PropertyType;
            Type maybeEnumType = Nullable.GetUnderlyingType(storedType) ?? storedType;
            this.setDelegate = maybeEnumType.GetTypeInfo().IsEnum
                ? (message, value) => write(message, value == null ? null : Enum.ToObject(enumType: maybeEnumType, value: value))
                : write;

            this.clearDelegate = CreateClearDelegate();
        }

        public FieldDescriptor Descriptor => descriptor;

        public void Clear(IMessage message) => clearDelegate(message);

        public object GetValue(IMessage message)
        {
            return getDelegate(message);
        }

        public bool HasValue(IMessage message)
        {
            return hasDelegate(message);
        }

        public void SetValue(IMessage message, object value) => setDelegate(message, value);

        private Action<IMessage> CreateClearDelegate()
        {
            if (descriptor.RealContainingOneof != null)
            {
                OneofAccessor oneofAccessor = descriptor.RealContainingOneof.Accessor;
                return message =>
                {
                    // Clearing a field that isn't the current case leaves the oneof alone.
                    if (oneofAccessor.GetCaseFieldDescriptor(message) == descriptor)
                    {
                        oneofAccessor.Clear(message);
                    }
                };
            }
            if (descriptor.IsMap)
            {
                return message => SetValue(message: message, value: ImmutableFieldCollections.NewMapLike(current: (IDictionary) GetValue(message)));
            }
            if (descriptor.IsRepeated)
            {
                return message => SetValue(message: message, value: ImmutableFieldCollections.EmptyLike(current: (IList) GetValue(message)));
            }
            if (descriptor.HasPresence)
            {
                return message => SetValue(message: message, value: null);
            }
            object defaultValue = SingleFieldAccessor.GetDefaultValue(descriptor);
            return message => SetValue(message: message, value: defaultValue);
        }
    }
}