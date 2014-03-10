#include "conn.hpp"
#include "stmt.hpp"

using namespace Eos;

Persistent<FunctionTemplate> Connection::constructor_;

void Connection::Init(Handle<Object> exports)  {
    EOS_DEBUG_METHOD();

    EosHandle::Init("Connection", constructor_, New);

    auto sig0 = Signature::New(constructor_);
    EOS_SET_METHOD(constructor_, "connect", Connection, Connect, sig0);
    EOS_SET_METHOD(constructor_, "driverConnect", Connection, DriverConnect, sig0);
    EOS_SET_METHOD(constructor_, "browseConnect", Connection, BrowseConnect, sig0);
    EOS_SET_METHOD(constructor_, "newStatement", Connection, NewStatement, sig0);
    EOS_SET_METHOD(constructor_, "nativeSql", Connection, NativeSql, sig0);
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

Handle<Value> Connection::NativeSql(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 1)
        return ThrowError("Connection::NativeSql requires an argument");

    WStringValue odbcSql(args[0]);
    if (!*odbcSql)
        return ThrowTypeError("The first argument must be a string or convertible to a string");

    SQLWCHAR buffer[4096];
    SQLINTEGER charsAvailable;

    auto ret = SQLNativeSqlW(
        GetHandle(),
        *odbcSql, odbcSql.length(),
        buffer, sizeof(buffer) / sizeof(buffer[0]),
        &charsAvailable);

    if (!SQL_SUCCEEDED(ret))
        return ThrowException(GetLastError());

    // Need more room for result (4095 chars not enough)
    // >= because if they are equal, the \0 will displace the last char
    if (charsAvailable >= sizeof(buffer) / sizeof(buffer[0])) {
        auto buffer = new(nothrow) SQLWCHAR[charsAvailable + 1];
        SQLINTEGER newCharsAvailable;

        if (!buffer)
            return ThrowError("Out of memory allocating space for native SQL");

        ret = SQLNativeSqlW(
            GetHandle(),
            *odbcSql, odbcSql.length(),
            buffer, charsAvailable, &newCharsAvailable);

        if (!SQL_SUCCEEDED(ret)) {
            delete[] buffer;
            return ThrowException(GetLastError());
        }

        if (newCharsAvailable > charsAvailable) {
            EOS_DEBUG(L"SQLNativeSql changed its mind");
            newCharsAvailable = charsAvailable;
        }

        auto result = StringFromTChar(buffer, newCharsAvailable);
        delete[] buffer;
        return result;
    } else {
        return StringFromTChar(buffer, charsAvailable);
    }
}

namespace { ClassInitializer<Connection> ci; }
