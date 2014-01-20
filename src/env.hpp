#pragma once 

#include "eos.hpp"

namespace Eos {
    struct Environment: ObjectWrap {
        Environment(SQLHENV hEnv);
        ~Environment();

        static void Init(Handle<Object> exports);
        static Handle<Value> New(const Arguments& args);
    
    public:

        SQLHENV GetHandle() const { return hEnv_.Value(); }

    public:

        Local<Value> GetLastError();

    private:
        ODBCHandle<SQL_HANDLE_ENV> hEnv_;

        static Persistent<FunctionTemplate> constructor_;
    };
}