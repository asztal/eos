#include "parameter.hpp"

#include <ctime>

using namespace Eos;

Persistent<FunctionTemplate> Parameter::constructor_;

void Parameter::Init(Handle<Object> exports) {
    constructor_ = Persistent<FunctionTemplate>::New(FunctionTemplate::New());
    constructor_->SetClassName(String::NewSymbol("Parameter"));
    constructor_->InstanceTemplate()->SetInternalFieldCount(1);

    auto sig0 = Signature::New(constructor_, 0, nullptr);

    EOS_SET_METHOD(constructor_, "getValue", Parameter, GetValue, sig0);
    
    EOS_SET_GETTER(constructor_, "buffer", Parameter, GetBuffer);
    EOS_SET_GETTER(constructor_, "bufferLength", Parameter, GetBufferLength);
    EOS_SET_GETTER(constructor_, "bytesInBuffer", Parameter, GetBytesInBuffer);
    EOS_SET_GETTER(constructor_, "index", Parameter, GetIndex);
    EOS_SET_GETTER(constructor_, "kind", Parameter, GetKind);
}

Parameter::Parameter
    ( SQLUSMALLINT parameterNumber
    , SQLSMALLINT inOutType
    , SQLSMALLINT sqlType
    , SQLSMALLINT cType
    , void* buffer
    , SQLLEN length
    , Handle<Object> bufferObject
    , SQLLEN indicator
    ) 
    : parameterNumber_(parameterNumber)
    , inOutType_(inOutType)
    , sqlType_(sqlType)
    , cType_(cType)
    , buffer_(buffer)
    , length_(length)
    , bufferObject_(Persistent<Object>::New(bufferObject))
    , indicator_(indicator)
{
    EOS_DEBUG_METHOD_FMT(L"buffer = 0x%p, length = %i", buffer, length);
}

Handle<Value> Parameter::GetBuffer() const {
    return bufferObject_;
}

Handle<Value> Parameter::GetBufferLength() const {
    return Integer::New(length_);
}

Handle<Value> Parameter::GetBytesInBuffer() const {
    if (inOutType_ != SQL_PARAM_OUTPUT && inOutType_ != SQL_PARAM_INPUT_OUTPUT)
        return ThrowError("Parameter::GetBytesInBuffer can only be called for bound output parameters");

    if (indicator_ == SQL_NULL_DATA)
        return Null();

    return Integer::New(BytesInBuffer());
}

Handle<Value> Parameter::GetIndex() const {
    return Integer::New(parameterNumber_);
}

Handle<Value> Parameter::GetKind() const {
    return Integer::New(inOutType_);
}

SQLLEN Parameter::BytesInBuffer() const {
    auto bytes = indicator_;

    // This should only happen for DAE input parameters
    // before the statement has been executed or before
    // the data has been supplied.
    if (indicator_ == SQL_DATA_AT_EXEC)
        return 0;

    if (indicator_ > length_ || indicator_ == SQL_NO_TOTAL)
        bytes = length_;

    // Reduce bytes to accommodate null terminator
    if (cType_ == SQL_C_CHAR && bytes == length_) {
        assert(bytes >= 1);
        bytes -= sizeof(char);
    } else if (cType_ == SQL_C_WCHAR && bytes == length_) {
        assert(bytes >= sizeof(SQLWCHAR));
        bytes -= sizeof(SQLWCHAR);
    }

    return bytes;
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

    SQLLEN GetDesiredBufferLength(SQLSMALLINT cType) {
        switch(cType) {
        case SQL_C_SLONG: return sizeof(long);
        case SQL_C_DOUBLE: return sizeof(double);
        case SQL_C_BIT: return sizeof(bool);
        case SQL_C_TYPE_TIMESTAMP: return sizeof(SQL_TIMESTAMP_STRUCT);
        default: return 0;
        }
    }

    SQLLEN FillInputBuffer(SQLSMALLINT cType, Handle<Value> jsValue, SQLPOINTER buffer, SQLLEN length) {
        switch(cType) {
        case SQL_C_SLONG:
            *reinterpret_cast<long*>(buffer) = jsValue->Int32Value();
            return sizeof(long);

        case SQL_C_DOUBLE:
            *reinterpret_cast<double*>(buffer) = jsValue->NumberValue();
            return sizeof(double);

        case SQL_C_BIT:
            *reinterpret_cast<bool*>(buffer) = jsValue->BooleanValue();
            return sizeof(bool);

        case SQL_C_TYPE_TIMESTAMP:
            if (!jsValue->IsDate())
                return 0;

            {
                long jsTime = static_cast<long>(jsValue.As<Date>()->NumberValue());
                time_t time = jsTime;

                tm tm = *gmtime(&time);
                auto ts = reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(buffer);
                ts->year = tm.tm_year + 1900;
                ts->month = tm.tm_mon + 1;
                ts->day = tm.tm_mday;
                ts->hour = tm.tm_hour;
                ts->minute = tm.tm_min;
                ts->second = tm.tm_sec;
                ts->fraction = (jsTime % 1000) * 100;
                return sizeof(*ts);
            }

        case SQL_C_CHAR:
            {
                String::Utf8Value val(jsValue);
                if (!*val)
                    return false;

                auto chars = min<size_t>(val.length(), length);
                strncpy(reinterpret_cast<char*>(buffer), *val, chars); 
                return chars;
            }

        case SQL_C_WCHAR:
            {
                WStringValue val(jsValue);
                if (!*val)
                    return false;

                auto chars = min<size_t>(val.length(), length / sizeof(**val));
                wcsncpy(reinterpret_cast<wchar_t*>(buffer), *val, chars); 
                return chars * sizeof(**val);
            }

        default: 
            return 0;
        }
    }

    bool AllocateBoundInputParameter(SQLSMALLINT cType, Handle<Value> jsValue, SQLPOINTER& buffer, SQLLEN& length, Handle<Object>& handle) {
        switch (cType) {
        case SQL_C_SLONG:
            if(!AllocatePrimitive<long>(jsValue->Int32Value(), buffer, handle))
                return false;
            length = sizeof(long);
            return true;

        case SQL_C_DOUBLE:
            if(!AllocatePrimitive<double>(jsValue->NumberValue(), buffer, handle))
                return false;
            length = sizeof(double);
            return true;

        case SQL_C_BIT:
            if(!AllocatePrimitive<bool>(jsValue->BooleanValue(), buffer, handle))
                return false;
            length = sizeof(bool);
            return true;

        case SQL_C_TYPE_TIMESTAMP:
            if (jsValue->IsDate()) {
                long jsTime = static_cast<long>(jsValue.As<Date>()->NumberValue());
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
            return true;

        case SQL_C_CHAR:
            if (jsValue->IsString()) {
                handle = JSBuffer::New(jsValue.As<String>(), String::NewSymbol("utf8"));
                if (handle.IsEmpty())
                    return false;
                JSBuffer::Unwrap(handle, buffer, length);
            } else {
                return false; 
            }
            return true;

        case SQL_C_WCHAR:
            if (jsValue->IsString()) {
                handle = JSBuffer::New(jsValue.As<String>(), String::NewSymbol("ucs2"));
                if (handle.IsEmpty())
                    return false;
                JSBuffer::Unwrap(handle, buffer, length);
            } else {
                return false; 
            }
            return true;

        case SQL_C_BINARY:
            if (Buffer::HasInstance(jsValue) 
                || (jsValue->IsObject() 
                    && JSBuffer::Constructor()->StrictEquals(jsValue.As<Object>()->GetConstructor()))) 
            {
                handle = jsValue.As<Object>();
                if(JSBuffer::Unwrap(handle, buffer, length))
                    return false;
            }
            return true;

        default:
            return false;
        }
    }

    bool AllocateOutputBuffer(SQLSMALLINT cType, SQLPOINTER& buffer, SQLLEN& length, Handle<Object>& handle) {
        length = GetDesiredBufferLength(cType);
        if (length <= 0)
            return false;

        handle = JSBuffer::New(length);
        if(auto msg = JSBuffer::Unwrap(handle, buffer, length)) {
            EOS_DEBUG(L"%hs\n", msg);
            return false;
        }

        return true;
    }
}

Handle<Value> Parameter::Marshal(
    SQLUSMALLINT parameterNumber, 
    SQLSMALLINT inOutType, 
    SQLSMALLINT sqlType,
    SQLSMALLINT decimalDigits, 
    Handle<Value> jsValue, 
    Handle<Object> handle
    ) 
{
    EOS_DEBUG_METHOD_FMT(L"fType = %i, digits = %i", inOutType, decimalDigits);

    assert(!jsValue.IsEmpty());

    auto cType = GetCTypeForSQLType(sqlType);

    SQLPOINTER buffer = nullptr;
    SQLLEN length = 0;
    SQLLEN indicator = 0;
    bool streamedInput = false, streamedOutput = false;

    if (!handle.IsEmpty())
        if(auto msg = JSBuffer::Unwrap(handle, buffer, length))
            return ThrowError(msg);

    // The two cases where input buffer is bound
    if ((inOutType == SQL_PARAM_INPUT || inOutType == SQL_PARAM_INPUT_OUTPUT) 
        && !jsValue->IsUndefined())
    {
        if (jsValue->IsNull()) {
            buffer = nullptr;
            length = 0;
            indicator = SQL_NULL_DATA;
        } else if(handle.IsEmpty()) {
            // It's an input parameter, and we have a value.
            if (!AllocateBoundInputParameter(cType, jsValue, buffer, length, handle))
                return ThrowError("Cannot allocate buffer for bound input or input/output parameter");
            indicator = length;
        } else {
            // Bound input parameter, check that buffer is big enough
            auto len2 = GetDesiredBufferLength(cType);
            if (length < len2)
                return ThrowError("The passed buffer is too small");

            // Now fill the buffer with the marhsalled value
            indicator = FillInputBuffer(cType, jsValue, buffer, length);
            if (!indicator)
                return ThrowError("Cannot place parameter value into buffer");
        }
    } else if (inOutType == SQL_PARAM_OUTPUT || inOutType == SQL_PARAM_INPUT_OUTPUT) {
        // It's an output parameter, in a bound buffer, or a DAE input parameter with a
        // bound output buffer.
        if (handle.IsEmpty()) {
            if (!AllocateOutputBuffer(cType, buffer, length, handle))
                return ThrowError("Cannot allocate buffer for output parameter");
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
        return ThrowError("Unknown parameter kind");
    }

    assert(buffer == nullptr || !handle.IsEmpty());

    auto param = new(nothrow) Parameter(parameterNumber, inOutType, sqlType, cType, buffer, length, handle, indicator);
    if (!param)
        return ThrowError("Out of memory allocating parameter structure");

    if (buffer == nullptr)
        param->buffer_ = param;

    auto obj = constructor_->GetFunction()->NewInstance();
    param->Wrap(obj);
    return obj;
}

Handle<Value> Parameter::GetValue(const Arguments& arg) {
    if (inOutType_ != SQL_PARAM_OUTPUT && inOutType_ != SQL_PARAM_INPUT_OUTPUT)
        return ThrowError("GetValue can only be called for bound output parameters");

    if (indicator_ == SQL_NULL_DATA)
        return Null();

    if (cType_ == SQL_C_BINARY) {
        assert(indicator_ <= length_ || indicator_ == SQL_NO_TOTAL);

        if (indicator_ == length_ || indicator_ == SQL_NO_TOTAL)
            return bufferObject_;
        return JSBuffer::Slice(bufferObject_, 0, indicator_);
    }

    return ConvertToJS(buffer_, BytesInBuffer(), cType_);
}

Parameter::~Parameter() {
    EOS_DEBUG_METHOD();
}

namespace { ClassInitializer<Parameter> init; } 
