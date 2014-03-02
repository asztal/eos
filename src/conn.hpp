#pragma once 

#include "eos.hpp"
#include "env.hpp"
#include "handle.hpp"

namespace Eos {
    struct Connection : EosHandle {
        static void Init(Handle<Object> exports);

        Connection(Environment* environment, SQLHDBC hDbc, HANDLE hEvent); 
        ~Connection();

        static const int HandleType = SQL_HANDLE_DBC;

    public:
        // JS methods
        static Handle<Value> New(const Arguments& args);
        Handle<Value> Connect(const Arguments& args);
        Handle<Value> BrowseConnect(const Arguments& args);
        Handle<Value> NewStatement(const Arguments& args);
        Handle<Value> NativeSql(const Arguments& args);
        Handle<Value> Disconnect(const Arguments& args);

    public:
        // Non-JS methods
        static Persistent<FunctionTemplate> Constructor() { return constructor_; }

    private:
        Environment* environment_;
        static Persistent<FunctionTemplate> constructor_;
    };
}
