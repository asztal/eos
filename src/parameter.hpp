#pragma once 

#include "eos.hpp"

namespace Eos {
    struct Parameter: ObjectWrap {
        Parameter(SQLUSMALLINT parameterNumber, SQLSMALLINT inOutType, SQLSMALLINT sqlType, SQLSMALLINT cType, void* buffer, SQLLEN length, Handle<Object> bufferObject, SQLLEN indicator);
        ~Parameter();
        
        static void Init(Handle<Object> exports);

        Handle<Value> GetValue(const Arguments& args);
        Handle<Value> GetValueLength(const Arguments& args);
        
        Handle<Value> GetIndex();
        Handle<Value> GetKind();

    public:

        static Parameter* NewDataAtExec(SQLLEN length);
        static Parameter* Marshal(SQLUSMALLINT parameterNumber, SQLSMALLINT inOutType, SQLSMALLINT decimalDigits, Handle<Value> jsVal, SQLSMALLINT sqlType);

        static Parameter* Unwrap(Handle<Object> obj) { return ObjectWrap::Unwrap<Parameter>(obj); }

        void* Buffer() const { return buffer_; }
        SQLLEN Length() const { return length_; }
        Handle<Object> BufferObject() { return bufferObject_; }
        
        SQLUSMALLINT ParameterNumber() const { return parameterNumber_; }
        SQLSMALLINT InOutType() const { return inOutType_; }
        SQLSMALLINT SQLType() const { return sqlType_; }
        SQLSMALLINT CType() const { return cType_; }

        SQLLEN* Indicator() { return &indicator_; }

        static Handle<FunctionTemplate> Constructor() { return constructor_; }

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
