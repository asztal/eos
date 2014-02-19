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
        Handle<Value> Prepare(const Arguments& args);
        Handle<Value> ExecDirect(const Arguments& args);
        Handle<Value> Execute(const Arguments& args);
        Handle<Value> Fetch(const Arguments& args);
        Handle<Value> GetData(const Arguments& args);
        Handle<Value> Cancel(const Arguments& args);
        Handle<Value> NumResultCols(const Arguments& args);
        Handle<Value> DescribeCol(const Arguments& args);

    public:

        // Non-JS methods
        static Persistent<FunctionTemplate> Constructor() { return constructor_; }

    private:
        Connection* connection_;

        static Persistent<FunctionTemplate> constructor_;
    };
}