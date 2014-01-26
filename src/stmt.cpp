#include "stmt.hpp"

using namespace Eos;

Persistent<FunctionTemplate> Statement::constructor_;

void Statement::Init(Handle<Object> exports) {
    EOS_DEBUG_METHOD();

    constructor_ = Persistent<FunctionTemplate>::New(FunctionTemplate::New(New));
    constructor_->SetClassName(String::NewSymbol("Statement"));
    constructor_->InstanceTemplate()->SetInternalFieldCount(1);
}

Handle<Value> Statement::New(const Arguments& args) {
    EOS_DEBUG_METHOD();

    HandleScope scope;

    if (args.Length() < 1)
        return ThrowError("Statement::New() requires at least one argument");

    if (!args.IsConstructCall()) {
        Handle<Value> argv[1] = { args[0] };
        return constructor_->GetFunction()->NewInstance(1, argv);
    }

    if (!Connection::Constructor()->HasInstance(args[0]))
        return ThrowTypeError("The first argument to Statement::New() must be a Connection");

    auto conn = ObjectWrap::Unwrap<Connection>(args[0]->ToObject());
    
    SQLHSTMT hStmt;

    auto ret = SQLAllocHandle(SQL_HANDLE_STMT, conn->GetHandle(), &hStmt);
    if (!SQL_SUCCEEDED(ret))
        return ThrowException(conn->GetLastError());

    (new Statement(hStmt, conn))->Wrap(args.Holder());

    return scope.Close(args.Holder());
}

Handle<Value> Statement::Free(const Arguments& args) {
    EOS_DEBUG_METHOD();

    auto ret = hStmt_.Free();
    if (!SQL_SUCCEEDED(ret))
        return ThrowException(GetLastError());

    return Undefined();
}

Statement::Statement(SQLHSTMT hStmt, Connection* conn) 
    : hStmt_(hStmt)
    , connection_(conn)
{
    EOS_DEBUG_METHOD();
}

Statement::~Statement() {
    EOS_DEBUG_METHOD();
}

namespace { ClassInitializer<Statement> c; }
