#include "env.hpp"
#include "conn.hpp"

using namespace Eos;

void Environment::Init(Handle<Object> exports) {
    constructor_ = Persistent<FunctionTemplate>::New(FunctionTemplate::New(New));

    constructor_->SetClassName(String::NewSymbol("Environment"));
    constructor_->InstanceTemplate()->SetInternalFieldCount(1);

    // The exported constructor can only be called on values made by the internal constructor.
    auto sig0 = Signature::New(constructor_);

    EOS_SET_METHOD(constructor_, "newConnection", Environment, NewConnection, sig0);
    EOS_SET_METHOD(constructor_, "dataSources", Environment, DataSources, sig0);
    EOS_SET_METHOD(constructor_, "drivers", Environment, Drivers, sig0);
    EOS_SET_METHOD(constructor_, "free", Environment, Free, sig0);

    exports->Set(String::NewSymbol("Environment"), constructor_->GetFunction(), ReadOnly);
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
        return ThrowError("Unable to allocate environment handle");
    
    ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3_80, SQL_IS_UINTEGER);
    if (!SQL_SUCCEEDED(ret)) {
        auto exception = Eos::GetLastError(SQL_HANDLE_ENV, hEnv);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return ThrowException(exception);
    }

    (new Environment(hEnv))->Wrap(args.Holder());
    return scope.Close(args.Holder());
}

namespace {
    Handle<Value> ThrowClosed() {
        return ThrowException(Exception::Error(String::New("The environment has been closed.")));
    }
}

Handle<Value> Environment::NewConnection(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (!hEnv_)
        return ThrowClosed();

    Handle<Value> argv[1] = { handle_ };
    return Connection::Constructor()->GetFunction()->NewInstance(1, argv);
}

Handle<Value> Environment::DataSources(const Arguments& args) {
    EOS_DEBUG_METHOD();

    SQLUSMALLINT direction = SQL_FETCH_FIRST;

    auto results = Array::New();

    if (args.Length() > 0) {
        Handle<String> type = args[0]->ToString();
        if (type->Equals(String::New("user")))
            direction = SQL_FETCH_FIRST_USER;
        else if (type->Equals(String::New("system")))
            direction = SQL_FETCH_FIRST_SYSTEM;
        else
            return ThrowError("The first argument must be 'system', 'user', or omitted");
    }

    uint32_t i = 0;
    SQLRETURN ret;

    wchar_t serverName[SQL_MAX_DSN_LENGTH + 1], description[256 + 1];
    SQLSMALLINT serverNameLength, descriptionLength;

    for(;;) {
        ret = SQLDataSourcesW(
            GetHandle(),
            direction,
            serverName, sizeof(serverName) / sizeof(wchar_t), &serverNameLength,
            description, sizeof(description) / sizeof(wchar_t), &descriptionLength);
    
        if (ret == SQL_NO_DATA)
            return results;

        if (ret == SQL_ERROR)
            return ThrowException(GetLastError());

        auto item = Object::New();
        item->Set(String::NewSymbol("server"), StringFromTChar(serverName, min(sizeof(serverName) / sizeof(wchar_t), (size_t)serverNameLength)));
        item->Set(String::NewSymbol("description"), StringFromTChar(description, min(sizeof(description) / sizeof(wchar_t), (size_t)descriptionLength)));
        results->Set(i++, item);

        direction = SQL_FETCH_NEXT;
    }
}

Handle<Value> Environment::Drivers(const Arguments& args) {
    EOS_DEBUG_METHOD();

    SQLUSMALLINT direction = SQL_FETCH_FIRST;

    auto results = Array::New();

    uint32_t i = 0;
    SQLRETURN ret;

    const int maxAttributesLength = 4096;
    wchar_t description[SQL_MAX_DSN_LENGTH + 1], driverAttributes[maxAttributesLength + 1];
    SQLSMALLINT descriptionLength, driverAttributesLength;

    for(;;) {
        ret = SQLDriversW(
            GetHandle(),
            direction,
            description, sizeof(description) / sizeof(wchar_t), &descriptionLength,
            driverAttributes, sizeof(driverAttributes) / sizeof(wchar_t), &driverAttributesLength);
    
        if (ret == SQL_NO_DATA)
            return results;

        if (ret == SQL_ERROR)
            return ThrowException(GetLastError());

        auto item = Object::New();
        item->Set(String::NewSymbol("name"), StringFromTChar(description, min(sizeof(description) / sizeof(wchar_t), (size_t)descriptionLength)));

        auto attributes = Array::New();
        
        wchar_t* str = driverAttributes;
        for (uint32_t j = 0;; j++) {
            auto attribute = StringFromTChar(str);
            if (attribute->Length() == 0)
                break;

            attributes->Set(j, attribute);
            str += wcslen(str) + 1;
        }

        item->Set(String::NewSymbol("attributes"), attributes);
        results->Set(i++, item);

        direction = SQL_FETCH_NEXT;
    }
}

// Frees the environment handle. This will fail with HY010 if there are still 
// connection handles allocated to this environment. Closing an environment
// twice is allowed.
Handle<Value> Environment::Free(const Arguments& args) {
    EOS_DEBUG_METHOD();

    auto ret = hEnv_.Free();
    if (!SQL_SUCCEEDED(ret))
        return ThrowException(GetLastError());

    return Undefined();
}

Environment::~Environment() {
    EOS_DEBUG_METHOD();
}

Persistent<FunctionTemplate> Environment::constructor_;

namespace { ClassInitializer<Environment> x; }