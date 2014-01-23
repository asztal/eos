#include "conn.hpp"

using namespace Eos;

Persistent<FunctionTemplate> Connection::constructor_;

void Connection::Init(Handle<Object> exports)  {
    EOS_DEBUG_METHOD();

    Handle<FunctionTemplate> sigAny1EnvArgs[1] = { Environment::Constructor() };
    auto sigAny1Env = Signature::New(Handle<FunctionTemplate>(), 1, sigAny1EnvArgs);

    constructor_ = Persistent<FunctionTemplate>::New(FunctionTemplate::New(New, Handle<Value>(), sigAny1Env));
    constructor_->SetClassName(String::NewSymbol("Connection"));
    constructor_->InstanceTemplate()->SetInternalFieldCount(1);
}

Connection::Connection(Environment* environment, SQLHDBC hDbc)
    : environment_(environment)
    , hDbc_(hDbc)
{
    EOS_DEBUG_METHOD();
}

Connection::~Connection() {
    EOS_DEBUG_METHOD();
}

Handle<Value> Connection::New(const Arguments& args) {
    EOS_DEBUG_METHOD();
    HandleScope scope;

    if (!args.IsConstructCall()) {
        Handle<Value> argv[1] = { args[0] };
        return constructor_->GetFunction()->NewInstance(1, argv);
    } else {
        if (args.Length() < 1)
            return ThrowException(OdbcError("Connection::New() requires at one argument"));

        if (!Environment::Constructor()->HasInstance(args[0]))
            return ThrowException(OdbcError("First argument must be an Environment"));

        auto env = ObjectWrap::Unwrap<Environment>(args[0]->ToObject());
        
        SQLHDBC hDbc;

        auto ret = SQLAllocHandle(SQL_HANDLE_DBC, env->GetHandle(), &hDbc);
        if (!SQL_SUCCEEDED(ret))
            return ThrowException(env->GetLastError());

        EOS_DEBUG(__FUNCTIONW__ L"(): hEnv=0x%p, hDbc=0x%p\n", env->GetHandle(), hDbc);

        auto conn = new Connection(env, hDbc);
        conn->Wrap(args.Holder());

        return scope.Close(args.Holder());
    }
}

namespace { ClassInitializer<Connection> c; }