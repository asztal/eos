#pragma once 

#include "eos.hpp"

namespace Eos {
    struct Environment: ObjectWrap {
        Environment(SQLHENV hEnv);
        ~Environment();

        static void Init(Handle<Object> exports);
        static Handle<Value> New(const Arguments& args);

    public:
        // JS methods
        Handle<Value> NewConnection(const Arguments& args);
        Handle<Value> DataSources(const Arguments& args);
        Handle<Value> Free(const Arguments& args);

    public:
        // Non-JS methods
        SQLHENV GetHandle() const { return hEnv_.Value(); }
        Local<Value> GetLastError() { return Eos::GetLastError(hEnv_); }
        static Persistent<FunctionTemplate> Constructor() { return constructor_; }

    private:
        Environment::Environment(const Environment&); // = delete

        ODBCHandle<SQL_HANDLE_ENV> hEnv_;

        static Persistent<FunctionTemplate> constructor_;
    };
}