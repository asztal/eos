#pragma once 

#include "eos.hpp"
#include "env.hpp"

namespace Eos {
    struct Connection : ObjectWrap {
        static void Init(Handle<Object> exports);

        Connection(Environment* environment, SQLHDBC hDbc); 
        ~Connection();

    public:
        // JS methods
        static Handle<Value> New(const Arguments& args);
        static Handle<Value> NewStatement(const Arguments& args);

    public:
        // Non-JS methods
        SQLHENV GetHandle() const { return hDbc_.Value(); }
        Local<Value> GetLastError() { return Eos::GetLastError(hDbc_); }
        static Persistent<FunctionTemplate> Constructor() { return constructor_; }

    private:
        Connection(const Connection&); // = delete;

        Environment* environment_;
        ODBCHandle<SQL_HANDLE_DBC> hDbc_;

        static Persistent<FunctionTemplate> constructor_;
    };
}
