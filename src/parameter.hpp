#pragma once 

#include "eos.hpp"

namespace Eos {
    struct Parameter: ObjectWrap {
        Parameter(SQLUSMALLINT parameterNumber, SQLSMALLINT inOutType, SQLSMALLINT sqlType, SQLSMALLINT cType, void* buffer, SQLLEN length, Handle<Object> bufferObject, SQLLEN indicator, bool autoWrap = false);
        ~Parameter();
        
        static void Init(Handle<Object> exports);

        NAN_GETTER(GetValue) const;
        NAN_SETTER(SetValue);

        NAN_GETTER(GetBytesInBuffer) const;
        NAN_SETTER(SetBytesInBuffer);

        SQLLEN BytesInBuffer() const;
        
        NAN_GETTER(GetBuffer) const;
        NAN_SETTER(SetBuffer);

        NAN_GETTER(GetBufferLength) const;
        NAN_GETTER(GetIndex) const;
        NAN_GETTER(GetKind) const;

    public:

        static const char* Marshal(
            SQLUSMALLINT parameterNumber, 
            SQLSMALLINT inOutType,
            SQLSMALLINT sqlType,
            SQLSMALLINT decimalDigits, 
            Handle<Value> jsVal,
            Handle<Object> bufferObject,
            Handle<Object>& result);

        static Parameter* Unwrap(Handle<Object> obj) { 
            return ObjectWrap::Unwrap<Parameter>(obj);
        }

        void Ref() { EOS_DEBUG_METHOD(); ObjectWrap::Ref(); }
        void Unref() { EOS_DEBUG_METHOD(); ObjectWrap::Unref(); }

        void* Buffer() const throw() { return buffer_; }
        SQLLEN Length() const throw() { return length_; }
        Handle<Object> BufferObject() const throw() { return NanNew(bufferObject_); }
        
        SQLUSMALLINT ParameterNumber() const throw() { return parameterNumber_; }
        SQLSMALLINT InOutType() const throw() { return inOutType_; }
        SQLSMALLINT SQLType() const throw() { return sqlType_; }
        SQLSMALLINT CType() const throw() { return cType_; }

        const SQLLEN& Indicator() const throw() { return indicator_; }
        SQLLEN& Indicator() throw() { return indicator_; }

        static Handle<FunctionTemplate> Constructor() { return NanNew(constructor_); }

    private:
        static Persistent<FunctionTemplate> constructor_;

        SQLSMALLINT sqlType_, cType_;
        SQLSMALLINT inOutType_;
        SQLUSMALLINT parameterNumber_;

        void* buffer_;
        SQLLEN length_, indicator_;

        Persistent<Object> bufferObject_;
    };
}
