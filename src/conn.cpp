#include "conn.hpp"
#include "stmt.hpp"

using namespace Eos;

namespace Eos {
    struct ConnectOperation : Operation<Connection, ConnectOperation> {
        using Operation::Operation;

        ConnectOperation::ConnectOperation(Handle<Value> connectionString)
            : connectionString_(connectionString)
        {
            EOS_DEBUG_METHOD();
            EOS_DEBUG(L"Connection string: %s\n", *connectionString_);
        }

        static Handle<Value> New(Connection* owner, const Arguments& args) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 3)
                return ThrowError("Too few arguments");

            if (!args[1]->IsString())
                return ThrowTypeError("Connection string should be a string");

            (new ConnectOperation(args[1]))->Wrap(args.Holder());
            return args.Holder();
        }

        static const char* Name() { return "ConnectOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            SQLSMALLINT cchCompleted; // Don't care

            return SQLDriverConnectW(
                Owner()->GetHandle(), 
                SQL_NULL_HANDLE,
                *connectionString_, connectionString_.length(),
                nullptr, 0, &cchCompleted,
                SQL_DRIVER_NOPROMPT);
        }

        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            GetCallback()->Call(Context::GetCurrent()->Global(), 0, 0);
        }

    protected:
        WStringValue connectionString_;
    };
}

Persistent<FunctionTemplate> Connection::constructor_;
Persistent<FunctionTemplate> Operation<Connection, ConnectOperation>::constructor_;

void Connection::Init(Handle<Object> exports)  {
    EOS_DEBUG_METHOD();

    constructor_ = Persistent<FunctionTemplate>::New(FunctionTemplate::New(New));
    constructor_->SetClassName(String::NewSymbol("Connection"));
    constructor_->InstanceTemplate()->SetInternalFieldCount(1);

    auto sig0 = Signature::New(constructor_);
    EOS_SET_METHOD(constructor_, "newStatement", Connection, NewStatement, sig0);
    EOS_SET_METHOD(constructor_, "free", Connection, Free, sig0);
    EOS_SET_METHOD(constructor_, "connect", Connection, Connect, sig0);

    ConnectOperation::Init(exports);
}

Connection::Connection(Environment* environment, SQLHDBC hDbc, HANDLE hEvent)
    : environment_(environment)
    , hDbc_(hDbc)
    , hEvent_(hEvent)
{
    EOS_DEBUG_METHOD();

    hWait_ = Eos::Wait(GetWaitHandle(), this);
}

Connection::~Connection() {
    if (hEvent_) {
        Eos::Unwait(hWait_);
        CloseHandle(hEvent_);
        hWait_ = nullptr;
        hEvent_ = nullptr;
    }

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

Handle<Value> Connection::Connect(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 2)
        return ThrowError("Connection::Connect() requires 2 arguments");

    if (!hDbc_)
        return ThrowError("This connection has been freed.");

    Handle<Value> argv[] = { handle_, args[0], args[1] };
    operation_ = Persistent<Object>::New(ConnectOperation::Constructor()->GetFunction()->NewInstance(3, argv));
    
    ObjectWrap::Unwrap<ConnectOperation>(operation_)->Begin();

    return Undefined();
}

Handle<Value> Connection::NewStatement(const Arguments& args) {
    EOS_DEBUG_METHOD();

    Handle<Value> argv[1] = { handle_ };
    return Statement::Constructor()->GetFunction()->NewInstance(1, argv);
}

Handle<Value> Connection::Free(const Arguments& args) {
    EOS_DEBUG_METHOD();

    auto ret = hDbc_.Free();
    if (!SQL_SUCCEEDED(ret))
        return ThrowException(GetLastError());

    if (hEvent_) {
        Eos::Unwait(hWait_);
        CloseHandle(hEvent_);
        hEvent_ = nullptr;
        hWait_ = nullptr;
    }

    return Undefined();
}

namespace { ClassInitializer<Connection> ci; }
