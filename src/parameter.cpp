#include "parameter.hpp"

#include <ctime>

using namespace Eos;

Persistent<FunctionTemplate> Parameter::constructor_;

void Parameter::Init(Handle<Object> exports) {
    constructor_ = Persistent<FunctionTemplate>::New(FunctionTemplate::New());
    constructor_->SetClassName(String::NewSymbol("Parameter"));
    constructor_->PrototypeTemplate()->SetInternalFieldCount(1);
}

Parameter::Parameter
    ( SQLUSMALLINT parameterNumber
    , SQLSMALLINT inOutType
    , SQLSMALLINT sqlType
    , SQLSMALLINT cType
    , void* buffer
    , SQLLEN length
    , Handle<Object> bufferObject
    ) 
    : parameterNumber_(parameterNumber)
    , inOutType_(inOutType)
    , sqlType_(sqlType)
    , cType_(cType)
    , buffer_(buffer)
    , length_(length)
    , bufferObject_(Persistent<Object>::New(bufferObject))
{
    EOS_DEBUG_METHOD_FMT(L"buffer = 0x%p, length = %i", buffer, length);
}

namespace {
    bool Allocate(SQLLEN length, SQLPOINTER& buffer, Handle<Object>& handle) {
        handle = JSBuffer::New(length);
        if (handle.IsEmpty()) {
            EOS_DEBUG(L"Failed to allocate parameter buffer\n");
            buffer = nullptr;
            return false;
        }

        JSBuffer::Unwrap(handle, buffer, length);
        return true;
    }

    template<class T>
    bool AllocatePrimitive(const T& value, SQLPOINTER& buffer, Handle<Object>& handle) {
        if (!Allocate(sizeof(T), buffer, handle))
            return false;

        *reinterpret_cast<T*>(buffer) = value;
        return true;
    }

    bool AllocateBoundInputParameter(SQLSMALLINT cType, Handle<Value> jsValue, SQLPOINTER& buffer, SQLLEN& length, Handle<Object>& handle) {
        switch (cType) {
        case SQL_C_SLONG:
            if(!AllocatePrimitive<long>(jsValue->Int32Value(), buffer, handle))
                return false;
            length = sizeof(long);
            break;

        case SQL_C_DOUBLE:
            if(!AllocatePrimitive<double>(jsValue->NumberValue(), buffer, handle))
                return false;
            length = sizeof(double);
            break;

        case SQL_C_BIT:
            if(!AllocatePrimitive<bool>(jsValue->BooleanValue(), buffer, handle))
                return false;
            length = sizeof(bool);
            break;

        case SQL_C_TYPE_TIMESTAMP: {
            if (jsValue->IsDate()) {
                long jsTime = jsValue.As<Date>()->NumberValue();
                time_t time = jsTime;

                tm tm = *gmtime(&time);
                SQL_TIMESTAMP_STRUCT ts = { 0 };
                ts.year = tm.tm_year + 1900;
                ts.month = tm.tm_mon + 1;
                ts.day = tm.tm_mday;
                ts.hour = tm.tm_hour;
                ts.minute = tm.tm_min;
                ts.second = tm.tm_sec;
                ts.fraction = (jsTime % 1000) * 100;

                if (!AllocatePrimitive<SQL_TIMESTAMP_STRUCT>(ts, buffer, handle))
                    return false;
            } else {
                return false;
            }

            break;
        }

        case SQL_C_CHAR:
            if (jsValue->IsString()) {
                handle = JSBuffer::New(jsValue.As<String>(), String::NewSymbol("utf8"));
                if (handle.IsEmpty())
                    return false;
                JSBuffer::Unwrap(handle, buffer, length);
            } else {
                return false; 
            }

            break;

        case SQL_C_WCHAR:
            if (jsValue->IsString()) {
                handle = JSBuffer::New(jsValue.As<String>(), String::NewSymbol("ucs2"));
                if (handle.IsEmpty())
                    return false;
                JSBuffer::Unwrap(handle, buffer, length);
            } else {
                return false; 
            }

        case SQL_C_BINARY:
            if (Buffer::HasInstance(jsValue) 
                || (jsValue->IsObject() 
                    && JSBuffer::Constructor()->StrictEquals(jsValue.As<Object>()->GetConstructor()))) 
            {
                handle = jsValue.As<Object>();
                if(JSBuffer::Unwrap(handle, buffer, length))
                    return false;
            }

        default:
            return false;
        }

        return true;
    }

    bool AllocateOutputBuffer(SQLSMALLINT cType, SQLPOINTER& buffer, SQLLEN& length, Handle<Object>& handle) {
        switch (cType) {
        case SQL_C_SLONG:
            length = sizeof(long);
            break;
        case SQL_C_DOUBLE:
            length = sizeof(double);
            break;
        case SQL_C_BIT:
            length = sizeof(bool);
            break;
        case SQL_C_TYPE_TIMESTAMP:
            length = sizeof(SQL_TIMESTAMP_STRUCT);
            break;
        default:
            return false;
        }

        handle = JSBuffer::New(length);
        if(JSBuffer::Unwrap(handle, buffer, length))
            return false;

        return true;
    }
}

Parameter* Parameter::Marshal(SQLUSMALLINT parameterNumber, SQLSMALLINT inOutType, SQLSMALLINT decimalDigits, Handle<Value> jsValue, SQLSMALLINT sqlType) {
    EOS_DEBUG_METHOD_FMT(L"fType = %i, digits = %i", inOutType, decimalDigits);

    assert(!jsValue.IsEmpty());

    auto cType = GetCTypeForSQLType(sqlType);

    Handle<Object> handle;
    SQLPOINTER buffer;
    SQLLEN length;
    SQLLEN indicator;
    bool streamedInput = false, streamedOutput = false;

    // The two cases where input buffer is bound
    if ((inOutType == SQL_PARAM_INPUT || inOutType == SQL_PARAM_INPUT_OUTPUT) 
        && !jsValue->IsUndefined())
    {
        if (jsValue->IsNull()) {
            buffer = nullptr;
            length = 0;
            indicator = SQL_NULL_DATA;
        } else {
            // It's an input parameter, and we have a value.
            if (!AllocateBoundInputParameter(cType, jsValue, buffer, length, handle))
                return nullptr;
            indicator = length;
        }
    } else if (inOutType == SQL_PARAM_OUTPUT || inOutType == SQL_PARAM_INPUT_OUTPUT) {
        // It's an output parameter, in a bound buffer.
        if (Buffer::HasInstance(jsValue) || JSBuffer::HasInstance(jsValue)) {
            if(JSBuffer::Unwrap(jsValue.As<Object>(), buffer, length))
                return nullptr;
        } else {
            if (!AllocateOutputBuffer(cType, buffer, length, handle))
                return nullptr;
        }

        indicator = length;
    } else if(inOutType == SQL_PARAM_OUTPUT_STREAM 
              || inOutType == SQL_PARAM_INPUT_OUTPUT_STREAM
              || (inOutType == SQL_PARAM_INPUT && jsValue->IsUndefined())) 
    {
        // Data at execution
        buffer = nullptr;
        length = 0;
        indicator = SQL_DATA_AT_EXEC;
    } else {
        EOS_DEBUG(L"Unknown inOutType %i", inOutType);
        return nullptr;
    }

    assert(!handle.IsEmpty());
    assert(buffer);

    auto param = new(nothrow) Parameter(parameterNumber, inOutType, sqlType, cType, buffer, length, handle);
    if (!param)
        return nullptr;

    if (buffer == nullptr)
        param->buffer_ = param;

    auto obj = constructor_->GetFunction()->NewInstance();
    param->Wrap(obj);
    return param;
}

// TODO GetValue() (for bound output parameters only)
// TODO GetLengthIndicator() (for bound output parameters only)
// TODO GetIndex()
// TODO GetKind()

Parameter::~Parameter() {
    EOS_DEBUG_METHOD();
}

namespace { ClassInitializer<Parameter> init; } 
