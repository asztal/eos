#include "eos.hpp"

namespace Eos {
    namespace Buffers {
        bool Allocate(
            SQLLEN length,
            SQLPOINTER& buffer,
            Handle<Object>& handle);

        template<class T>
        bool AllocatePrimitive(const T& value, SQLPOINTER& buffer, Handle<Object>& handle) {
            if (!Allocate(sizeof(T), buffer, handle))
                return false;

            *reinterpret_cast<T*>(buffer) = value;
            return true;
        }

        SQLLEN GetDesiredBufferLength(
            SQLSMALLINT cType);

        SQLLEN FillInputBuffer(
            SQLSMALLINT cType,
            Handle<Value> jsValue,
            SQLPOINTER buffer, SQLLEN length);

        bool AllocateBoundInputParameter(
            SQLSMALLINT cType,
            Handle<Value> jsValue,
            SQLPOINTER& buffer, SQLLEN& length,
            Handle<Object>& handle);

        bool AllocateOutputBuffer(
            SQLSMALLINT cType, 
            SQLPOINTER& buffer, SQLLEN& length,
            Handle<Object>& handle);
    }
}