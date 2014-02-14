#include "conn.hpp"
#include "stmt.hpp"

using namespace Eos;

Persistent<FunctionTemplate> Connection::constructor_;

void Connection::Init(Handle<Object> exports)  {
    EOS_DEBUG_METHOD();

    EosHandle::Init("Connection", constructor_, New);

    auto sig0 = Signature::New(constructor_);
    EOS_SET_METHOD(constructor_, "newStatement", Connection, NewStatement, sig0);
    EOS_SET_METHOD(constructor_, "connect", Connection, Connect, sig0);
    EOS_SET_METHOD(constructor_, "browseConnect", Connection, BrowseConnect, sig0);
    EOS_SET_METHOD(constructor_, "disconnect", Connection, Disconnect, sig0);
}

Connection::Connection(Environment* environment, SQLHDBC hDbc, HANDLE hEvent)
    : environment_(environment)
    , EosHandle(SQL_HANDLE_DBC, hDbc, hEvent)
{
    EOS_DEBUG_METHOD();
}

Connection::~Connection() {
    EOS_DEBUG_METHOD();
}

Handle<Value> Connection::New(const Arguments& args) {
    EOS_DEBUG_METHOD();
    
    HandleScope scope;

    if (args.Length() < 1)
        return scope.Close(ThrowException(OdbcError("Connection::New() requires at least one argument")));

    if (!args.IsConstructCall()) {
        Handle<Value> argv[1] = { args[0] };
        return scope.Close(constructor_->GetFunction()->NewInstance(1, argv));
    } 
    
    if (!Environment::Constructor()->HasInstance(args[0]))
        return scope.Close(ThrowException(OdbcError("First argument must be an Environment")));

    auto env = ObjectWrap::Unwrap<Environment>(args[0]->ToObject());
        
    SQLHDBC hDbc;

    auto ret = SQLAllocHandle(SQL_HANDLE_DBC, env->GetHandle(), &hDbc);
    if (!SQL_SUCCEEDED(ret))
        return scope.Close(ThrowException(env->GetLastError()));

    ret = SQLSetConnectAttrW(
        hDbc, 
        SQL_ATTR_ASYNC_DBC_FUNCTIONS_ENABLE, 
        (SQLPOINTER)SQL_ASYNC_DBC_ENABLE_ON, 
        SQL_IS_INTEGER);

    if (!SQL_SUCCEEDED(ret)) {
        auto error = Eos::GetLastError(SQL_HANDLE_DBC, hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        return ThrowException(error);
    }

    auto hEvent = CreateEventW(nullptr, false, false, nullptr);
    if (!hEvent) {
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        return scope.Close(ThrowError("Unable to create wait handle"));
    }

    ret = SQLSetConnectAttrW(
        hDbc, 
        SQL_ATTR_ASYNC_DBC_EVENT, 
        hEvent, 
        SQL_IS_POINTER);

    if (!SQL_SUCCEEDED(ret)) {
        auto error = Eos::GetLastError(SQL_HANDLE_DBC, hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        CloseHandle(hEvent);
        return ThrowException(error);
    }

    (new Connection(env, hDbc, hEvent))->Wrap(args.Holder());

    return scope.Close(args.Holder());
}

Handle<Value> Connection::NewStatement(const Arguments& args) {
    EOS_DEBUG_METHOD();

    Handle<Value> argv[1] = { handle_ };
    return Statement::Constructor()->GetFunction()->NewInstance(1, argv);
}

namespace { ClassInitializer<Connection> ci; }
