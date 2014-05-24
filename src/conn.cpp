#include "conn.hpp"
#include "stmt.hpp"

using namespace Eos;

Persistent<FunctionTemplate> Connection::constructor_;

void Connection::Init(Handle<Object> exports) {
    EOS_DEBUG_METHOD();

    EosHandle::Init("Connection", constructor_, New);

    auto sig0 = NanNew<Signature>(Constructor());
    EOS_SET_METHOD(Constructor(), "connect", Connection, Connect, sig0);
    EOS_SET_METHOD(Constructor(), "driverConnect", Connection, DriverConnect, sig0);
    EOS_SET_METHOD(Constructor(), "browseConnect", Connection, BrowseConnect, sig0);
    EOS_SET_METHOD(Constructor(), "newStatement", Connection, NewStatement, sig0);
    EOS_SET_METHOD(Constructor(), "nativeSql", Connection, NativeSql, sig0);
    EOS_SET_METHOD(Constructor(), "disconnect", Connection, Disconnect, sig0);
}

Connection::Connection(Eos::Environment* environment, SQLHDBC hDbc EOS_ASYNC_ONLY_ARG(HANDLE hEvent))
    : environment_(environment)
    , EosHandle(SQL_HANDLE_DBC, hDbc EOS_ASYNC_ONLY_ARG(hEvent))
{
    EOS_DEBUG_METHOD();
}

Connection::~Connection() {
    EOS_DEBUG_METHOD();
}

NAN_METHOD(Connection::New) {
    EOS_DEBUG_METHOD();
    
    NanScope();

    if (args.Length() < 1)
        return NanThrowError(OdbcError("Connection::New() requires at least one argument"));

    if (!args.IsConstructCall()) {
        Handle<Value> argv[1] = { args[0] };
        NanReturnValue(Constructor()->GetFunction()->NewInstance(1, argv));
    } 
    
    if (!Environment::Constructor()->HasInstance(args[0]))
        return NanThrowError(OdbcError("First argument must be an Environment"));

    auto env = ObjectWrap::Unwrap<Environment>(args[0]->ToObject());
        
    SQLHDBC hDbc;

    auto ret = SQLAllocHandle(SQL_HANDLE_DBC, env->GetHandle(), &hDbc);
    if (!SQL_SUCCEEDED(ret))
        return NanThrowError(env->GetLastError());
    
#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
    // Attempt to enable asynchronous notifications
    HANDLE hEvent = nullptr;

    ret = SQLSetConnectAttrW(
        hDbc, 
        SQL_ATTR_ASYNC_DBC_FUNCTIONS_ENABLE, 
        (SQLPOINTER)SQL_ASYNC_DBC_ENABLE_ON, 
        SQL_IS_INTEGER);

    // If asynchronous notifications are supported
    if (SQL_SUCCEEDED(ret)) {
        hEvent = CreateEventW(nullptr, false, false, nullptr);
        if (!hEvent) {
            SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
            return NanThrowError("Unable to create wait handle");
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
            return NanThrowError(error);
        }
    }
#endif

    (new Connection(env, hDbc EOS_ASYNC_ONLY_ARG(hEvent)))->Wrap(args.Holder());

    NanReturnValue(args.Holder());
}

NAN_METHOD(Connection::NewStatement) {
    EOS_DEBUG_METHOD();

    Handle<Value> argv[1] = { NanObjectWrapHandle(this) };
    NanReturnValue(Statement::Constructor()->GetFunction()->NewInstance(1, argv));
}

NAN_METHOD(Connection::NativeSql) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 1)
        return NanThrowError("Connection::NativeSql requires an argument");

    WStringValue odbcSql(args[0]);
    if (!*odbcSql)
        return NanThrowTypeError("The first argument must be a string or convertible to a string");

    SQLWCHAR buffer[4096];
    SQLINTEGER charsAvailable;

    auto ret = SQLNativeSqlW(
        GetHandle(),
        *odbcSql, odbcSql.length(),
        buffer, sizeof(buffer) / sizeof(buffer[0]),
        &charsAvailable);

    if (!SQL_SUCCEEDED(ret))
        return NanThrowError(GetLastError());

    // Need more room for result (4095 chars not enough)
    // >= because if they are equal, the \0 will displace the last char
    if (charsAvailable >= sizeof(buffer) / sizeof(buffer[0])) {
        auto buffer = new(nothrow) SQLWCHAR[charsAvailable + 1];
        SQLINTEGER newCharsAvailable;

        if (!buffer)
            return NanThrowError("Out of memory allocating space for native SQL");

        ret = SQLNativeSqlW(
            GetHandle(),
            *odbcSql, odbcSql.length(),
            buffer, charsAvailable, &newCharsAvailable);

        if (!SQL_SUCCEEDED(ret)) {
            delete[] buffer;
            return NanThrowError(GetLastError());
        }

        if (newCharsAvailable > charsAvailable) {
            EOS_DEBUG(L"SQLNativeSql changed its mind");
            newCharsAvailable = charsAvailable;
        }

        auto result = StringFromTChar(buffer, newCharsAvailable);
        delete[] buffer;
        NanReturnValue(result);
    } else {
        NanReturnValue(StringFromTChar(buffer, charsAvailable));
    }
}

#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
void Connection::DisableAsynchronousNotifications() {
    EOS_DEBUG_METHOD();

    auto ret = SQLSetConnectAttrW(
        GetHandle(),
        SQL_ATTR_ASYNC_DBC_EVENT,
        nullptr,
        SQL_IS_POINTER);

    if (!SQL_SUCCEEDED(ret))
        EOS_DEBUG(L"Failed to unset the connection's event handle");

    ret = SQLSetConnectAttrW(
        GetHandle(),
        SQL_ATTR_ASYNC_DBC_FUNCTIONS_ENABLE,
        (SQLPOINTER)SQL_ASYNC_DBC_ENABLE_OFF,
        SQL_IS_INTEGER);

    if (!SQL_SUCCEEDED(ret))
        EOS_DEBUG(L"Failed to turn off asynchronous notifications");
}
#endif

namespace { ClassInitializer<Connection> ci; }
