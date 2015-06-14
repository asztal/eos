#include "parameter.hpp"
#include "buffer.hpp"

using namespace Eos;
using namespace Eos::Buffers;

Persistent<FunctionTemplate> Parameter::constructor_;

void Parameter::Init(Handle<Object> exports) {
    NanAssignPersistent(constructor_, NanNew<FunctionTemplate>());
    Constructor()->SetClassName(NanSymbol("Parameter"));
    Constructor()->InstanceTemplate()->SetInternalFieldCount(1);

    auto sig0 = NanNew<Signature>(Constructor());

    EOS_SET_ACCESSOR(Constructor(), "value", Parameter, GetValue, SetValue);
    EOS_SET_ACCESSOR(Constructor(), "bytesInBuffer", Parameter, GetBytesInBuffer, SetBytesInBuffer);
    EOS_SET_ACCESSOR(Constructor(), "buffer", Parameter, GetBuffer, SetBuffer);
    EOS_SET_GETTER(Constructor(), "bufferLength", Parameter, GetBufferLength);
    EOS_SET_GETTER(Constructor(), "index", Parameter, GetIndex);
    EOS_SET_GETTER(Constructor(), "kind", Parameter, GetKind);
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
    , bool autoWrap
    ) 
    : parameterNumber_(parameterNumber)
    , inOutType_(inOutType)
    , sqlType_(sqlType)
    , cType_(cType)
    , buffer_(buffer)
    , length_(length)
    , indicator_(indicator)
{
    EOS_DEBUG_METHOD_FMT(L"buffer = 0x%p, length = %i", buffer, length);

    NanAssignPersistent(bufferObject_, bufferObject);

    if (autoWrap)
        Wrap(Constructor()->GetFunction()->NewInstance());
}

NAN_GETTER(Parameter::GetBuffer) const {
    EosMethodReturnValue(NanNew(bufferObject_));
}

NAN_SETTER(Parameter::SetBuffer) {
    if (inOutType_ != SQL_PARAM_INPUT) {
        NanThrowError("Cannot set buffer for input/output or output parameters");
        return;
    }

    if (cType_ != SQL_C_CHAR && cType_ != SQL_C_BINARY && cType_ != SQL_C_WCHAR) {
        NanThrowError("Can only set buffer for binary or string parameters");
        return;
    }
    
    if (!JSBuffer::HasInstance(value)) {
        NanThrowError("Specified buffer must be a node Buffer object");
        return;
    }
        
    if (auto msg = JSBuffer::Unwrap(value.As<Object>(), buffer_, length_)) {
        NanThrowError(msg);
        return;
    }
}

NAN_GETTER(Parameter::GetBufferLength) const {
    EosMethodReturnValue(NanNew<Number>(length_));
}

NAN_GETTER(Parameter::GetBytesInBuffer) const {
    if (inOutType_ != SQL_PARAM_OUTPUT && inOutType_ != SQL_PARAM_INPUT_OUTPUT)
        NanReturnUndefined();

    if (indicator_ == SQL_NULL_DATA)
        NanReturnNull();

    EosMethodReturnValue(NanNew<Number>(BytesInBuffer()));
}

NAN_SETTER(Parameter::SetBytesInBuffer) {
    if (inOutType_ != SQL_PARAM_INPUT && inOutType_ != SQL_PARAM_INPUT_OUTPUT && inOutType_ != SQL_PARAM_INPUT_OUTPUT_STREAM) {
        NanThrowError("Cannot set bytesInBuffer for output parameters");
        return;
    }

    if (cType_ != SQL_C_CHAR && cType_ != SQL_C_WCHAR && cType_ != SQL_C_BINARY) {
        NanThrowError("Cannot set bytesInBuffer for output parameters");
        return;
    }

    if (indicator_ == SQL_DATA_AT_EXEC) {
        NanThrowError("Cannot set bytesInBuffer because there is no buffer");
        return;
    }

    auto bytes = value->IntegerValue();
    if (bytes < 0 || bytes > length_) {
        NanThrowError("bytesInBuffer must be non-negative and less than or equal to the buffer length");
        return;
    }

    indicator_ = bytes;
}

NAN_GETTER(Parameter::GetIndex) const {
    EosMethodReturnValue(NanNew<Integer>(parameterNumber_));
}

NAN_GETTER(Parameter::GetKind) const {
    EosMethodReturnValue(NanNew<Integer>(inOutType_));
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

    return bytes;
}

const char* Parameter::Marshal(
    SQLUSMALLINT parameterNumber, 
    SQLSMALLINT inOutType, 
    SQLSMALLINT sqlType,
    SQLSMALLINT decimalDigits, 
    Handle<Value> jsValue, 
    Handle<Object> handle,
    Handle<Object>& result) 
{
    EOS_DEBUG_METHOD_FMT(L"fType = %i, digits = %i", inOutType, decimalDigits);

    assert(!jsValue.IsEmpty());

    auto cType = GetCTypeForSQLType(sqlType);

    SQLPOINTER buffer = nullptr;
    SQLLEN length = 0;
    SQLLEN indicator = 0;

    if (!handle.IsEmpty())
        if(auto msg = JSBuffer::Unwrap(handle, buffer, length))
            return msg;

    // The two cases where input buffer is bound
    if ((inOutType == SQL_PARAM_INPUT || inOutType == SQL_PARAM_INPUT_OUTPUT) 
        && !jsValue->IsUndefined())
    {
        if (jsValue->IsNull()) {
            buffer = nullptr;
            length = 0;
            indicator = SQL_NULL_DATA;
        } else if (cType == SQL_C_BINARY && handle.IsEmpty() && JSBuffer::HasInstance(jsValue)) {
            handle = jsValue.As<Object>();

            if (auto msg = JSBuffer::Unwrap(handle, buffer, length))
                return msg;

            indicator = length;
        } else if (handle.IsEmpty()) {
            // It's an input parameter, and we have a value.
            if (!AllocateBoundInputParameter(cType, jsValue, buffer, length, handle))
                return "Cannot allocate buffer for bound input or input/output parameter";
            indicator = length;
        } else {
            // Bound input parameter, check that buffer is big enough
            auto len2 = GetDesiredBufferLength(cType);
            if (length < len2)
                return "The passed buffer is too small";

            // Now fill the buffer with the marhsalled value
            indicator = FillInputBuffer(cType, jsValue, buffer, length);
            if (!indicator)
                return "Cannot place parameter value into buffer";
        }
    } else if (inOutType == SQL_PARAM_OUTPUT || inOutType == SQL_PARAM_INPUT_OUTPUT) {
        if (inOutType == SQL_PARAM_INPUT_OUTPUT)
            return "Cannot bind this type of parameter with data-at-execution yet: see https://github.com/lee-houghton/eos/issues/5";

        // It's an output parameter, in a bound buffer, or a DAE input parameter with a
        // bound output buffer.
        if (handle.IsEmpty()) {
            if (!AllocateOutputBuffer(cType, buffer, length, handle))
                return "Cannot allocate buffer for output parameter";
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
        return "Unknown parameter kind";
    }

    assert(buffer == nullptr || !handle.IsEmpty());

    auto param = new(nothrow) Parameter(parameterNumber, inOutType, sqlType, cType, buffer, length, handle, indicator);
    if (!param)
        return "Out of memory allocating parameter structure";

    if (buffer == nullptr)
        param->buffer_ = param;

    result = Constructor()->GetFunction()->NewInstance();
    param->Wrap(result);

    return nullptr;
}

NAN_GETTER(Parameter::GetValue) const {
    if (inOutType_ != SQL_PARAM_OUTPUT && inOutType_ != SQL_PARAM_INPUT_OUTPUT)
        return NanThrowError("GetValue can only be called for bound output parameters");

    if (indicator_ == SQL_NULL_DATA)
        NanReturnNull();

    if (cType_ == SQL_C_BINARY) {
        assert(indicator_ >= 0 || indicator_ == SQL_NO_TOTAL);

        if (indicator_ >= length_ || indicator_ == SQL_NO_TOTAL)
            EosMethodReturnValue(NanNew(bufferObject_));
        EosMethodReturnValue(JSBuffer::Slice(NanNew(bufferObject_), 0, indicator_));
    }

    EosMethodReturnValue(ConvertToJS(buffer_, indicator_, length_, cType_));
}

NAN_SETTER(Parameter::SetValue) {
    if (value->IsNull()) {
        indicator_ = SQL_NULL_DATA;
        return;
    }

    if (bufferObject_.IsEmpty()) {
	Local<Object> buf = NanNew(bufferObject_);
        if (AllocateBoundInputParameter(cType_, value, buffer_, length_, buf)) {
	    NanAssignPersistent(bufferObject_, buf);
	} else {
            NanThrowError("Cannot allocate buffer for parameter data");
            return;
        }
        indicator_ = length_;
    } else {
        auto desiredLength = GetDesiredBufferLength(cType_);
        if (length_ < desiredLength) {
            NanThrowError("The parameter data buffer is too small to contain the value");
            return;
        }

        indicator_ = FillInputBuffer(cType_, value, buffer_, length_);
        if (!indicator_) {
            NanThrowError("Cannot place parameter value into buffer");
            return;
        }
    }
}

Parameter::~Parameter() {
    EOS_DEBUG_METHOD();

    if (!bufferObject_.IsEmpty())
        NanDisposePersistent(bufferObject_);
}

namespace { ClassInitializer<Parameter> init; } 
