#include "parameter.hpp"
#include "buffer.hpp"

using namespace Eos;
using namespace Eos::Buffers;

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
