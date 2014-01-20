#include "env.hpp"

using namespace Eos;

void Environment::Init(Handle<Object> exports) {
    auto ft = FunctionTemplate::New(New);
    constructor_ = Persistent<FunctionTemplate>::New(ft);

    ft->SetClassName(String::NewSymbol("Environment"));
    ft->InstanceTemplate()->SetInternalFieldCount(1);

    // The exported constructor can only be called on values made by the internal constructor.
    auto sig = Signature::New(ft);
    auto exportedConstructor = FunctionTemplate::New(New, Handle<Value>(), sig);

    exports->Set(String::NewSymbol("Environment"), ft->GetFunction(), ReadOnly);
}

Environment::Environment(SQLHENV hEnv) : hEnv_(hEnv) {
    EOS_DEBUG_METHOD();
}

Handle<Value> Environment::New(const Arguments& args) {
    EOS_DEBUG_METHOD();

    HandleScope scope;

    if (!args.IsConstructCall())
        return scope.Close(constructor_->GetFunction()->NewInstance());

    SQLHENV hEnv;
    auto ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    if (!SQL_SUCCEEDED(ret))
        return ThrowException(Exception::Error(String::New("Unable to allocate environment handle")));
    
    ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3_80, SQL_IS_UINTEGER);
    if (!SQL_SUCCEEDED(ret)) {
        Local<Value> exception = Eos::GetLastError(SQL_HANDLE_ENV, hEnv);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return ThrowException(exception);
    }

    Environment* env = new Environment(hEnv);
    env->Wrap(args.Holder());

    return scope.Close(args.Holder());
}

Environment::~Environment() {
    EOS_DEBUG_METHOD();
}

Local<Value> Environment::GetLastError() {
    return Eos::GetLastError(hEnv_);
}

Persistent<FunctionTemplate> Environment::constructor_;

namespace { ClassInitializer<Environment> x; }