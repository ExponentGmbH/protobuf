using System;
using System.Collections;
using System.Collections.Generic;
using Google.Protobuf.Collections;

namespace Google.Protobuf.Reflection
{
    /// <summary>
    /// Reflection cannot write into the collections of generated records and structs: their repeated fields are
    /// ImmutableArray and their maps are ImmutableDictionary or a ReadOnlyDictionary. Each writer has to build a new
    /// collection and store it through the field's setter. This library does not reference
    /// System.Collections.Immutable, so these work on the instance the message holds. Each of those types has the
    /// field's element type, or key and value types, as its generic arguments.
    /// </summary>
    internal static class ImmutableFieldCollections
    {
        internal static IDictionary NewMapLike(IDictionary current) =>
            (IDictionary) Activator.CreateInstance(type: typeof(MapField<,>).MakeGenericType(current.GetType().GetGenericArguments()));

        internal static IDictionary CopyMap(IDictionary current)
        {
            IDictionary copy = NewMapLike(current: current);
            foreach (object key in current.Keys)
            {
                copy[key] = current[key];
            }
            return copy;
        }

        internal static IList NewRepeatedFieldLike(IList current) =>
            (IList) Activator.CreateInstance(type: typeof(RepeatedField<>).MakeGenericType(current.GetType().GetGenericArguments()));

        internal static object EmptyLike(IList current) =>
            current.GetType().GetField(name: "Empty").GetValue(obj: null);

        /// <returns>A new collection of <paramref name="current"/>'s type, holding its items and then <paramref name="items"/>.</returns>
        internal static object Append(IList current, IEnumerable items)
        {
            Type enumerable = typeof(IEnumerable<>).MakeGenericType(current.GetType().GetGenericArguments());
            return current.GetType()
                .GetMethod(name: "AddRange", types: new[] { enumerable })
                .Invoke(obj: current, parameters: new object[] { items });
        }
    }
}
