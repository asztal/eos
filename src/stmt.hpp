#pragma once

#include "eos.hpp"
#include "conn.hpp"
#include "handle.hpp"

namespace Eos {
    struct Statement: EosHandle {
        static void Init(Handle<Object> exports);

        Statement(SQLHSTMT hStmt, Connection* connection, HANDLE hEvent);
        ~Statement();

        static const int HandleType = SQL_HANDLE_STMT;

    public:

        static Handle<Value> New(const Arguments& args);
        Handle<Value> ExecDirect(const Arguments& args);

    public:

        // Non-JS methods
        static Persistent<FunctionTemplate> Constructor() { return constructor_; }

    private:
        Connection* connection_;

        static Persistent<FunctionTemplate> constructor_;
    };
}