#pragma once 

#include "eos.hpp"
#include "env.hpp"
#include "handle.hpp"

namespace Eos {
    struct Connection : EosHandle {
        static void Init(Handle<Object> exports);

        Connection(Environment* environment, SQLHDBC hDbc EOS_ASYNC_ONLY_ARG(HANDLE hEvent)); 
        ~Connection();

        static const int HandleType = SQL_HANDLE_DBC;

    public:
        // JS methods
        static NAN_METHOD(New);
        NAN_METHOD(Connect);
        NAN_METHOD(DriverConnect);
        NAN_METHOD(BrowseConnect);
        NAN_METHOD(NewStatement);
        NAN_METHOD(NativeSql);
        NAN_METHOD(Disconnect);

    public:
        // Non-JS methods
        static Handle<FunctionTemplate> Constructor() { return NanNew(constructor_); }
        void DisableAsynchronousNotifications();

    private:
        Eos::Environment* environment_;
        static Persistent<FunctionTemplate> constructor_;
    };
}
