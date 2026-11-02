#region Copyright notice and license
// Protocol Buffers - Google's data interchange format
// Copyright 2015 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
#endregion

using System;
using System.Collections;
using System.Reflection;

namespace Google.Protobuf.Reflection
{
    /// <summary>
    /// Accessor for repeated fields.
    /// </summary>
    internal sealed class RepeatedFieldAccessor : FieldAccessorBase
    {
        // Only generated records have a setter: stock generated code exposes a mutable collection without one.
        private readonly Action<IMessage, object> maybeSetter;

        internal RepeatedFieldAccessor(PropertyInfo property, FieldDescriptor descriptor) : base(property, descriptor)
        {
            MethodInfo maybeSetMethod = property.GetSetMethod();
            maybeSetter = maybeSetMethod != null ? ReflectionUtil.CreateActionIMessageObject(maybeSetMethod) : null;
        }

        public override void Clear(IMessage message)
        {
            IList list = (IList) GetValue(message);
            if (list.IsReadOnly)
            {
                SetValue(message, ImmutableFieldCollections.EmptyLike(current: list));
                return;
            }
            list.Clear();
        }

        public override bool HasValue(IMessage message)
        {
            throw new InvalidOperationException("HasValue is not implemented for repeated fields");
        }

        public override void SetValue(IMessage message, object value)
        {
            if (maybeSetter == null)
            {
                throw new InvalidOperationException("SetValue is not implemented for repeated fields");
            }
            maybeSetter(message, value);
        }
    }
}
