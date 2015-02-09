#include "env.hpp"
#include "conn.hpp"

using namespace Eos;

void Eos::Environment::Init(Handle<Object> exports) {
    EosHandle::Init("Environment", constructor_, New);

    // The exported constructor can only be called on values made by the internal constructor.
    auto sig0 = NanNew<Signature>(Constructor());

    EOS_SET_METHOD(Constructor(), "newConnection", Environment, NewConnection, sig0);
    EOS_SET_METHOD(Constructor(), "dataSources", Environment, DataSources, sig0);
    EOS_SET_METHOD(Constructor(), "drivers", Environment, Drivers, sig0);
    
    exports->Set(NanSymbol("Environment"), Constructor()->GetFunction() IF_NODE_12(, EOS_COMMA ReadOnly));
}

Eos::Environment::Environment(SQLHENV hEnv) 
    : EosHandle(SQL_HANDLE_ENV, hEnv EOS_ASYNC_ONLY_ARG(nullptr)) 
{
    EOS_DEBUG_METHOD();
}

NAN_METHOD(Eos::Environment::New) {
    EOS_DEBUG_METHOD();

    NanScope();

    if (!args.IsConstructCall())
        NanReturnValue(Constructor()->GetFunction()->NewInstance());

    SQLHENV hEnv;
    auto ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
    if (!SQL_SUCCEEDED(ret))
        return NanThrowError("Unable to allocate environment handle");
    
    ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3_80, SQL_IS_UINTEGER);
    if (!SQL_SUCCEEDED(ret)) {
        auto exception = Eos::GetLastError(SQL_HANDLE_ENV, hEnv);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return NanThrowError(exception);
    }

    (new Environment(hEnv))->Wrap(args.Holder());
    NanReturnValue(args.Holder());
}

namespace {
    _NAN_METHOD_RETURN_TYPE ThrowClosed() {
        return NanThrowError(OdbcError("The environment has been closed."));
    }
}

NAN_METHOD(Eos::Environment::NewConnection) {
    EOS_DEBUG_METHOD();

    if (!IsValid())
        return ThrowClosed();

    Handle<Value> argv[1] = { NanObjectWrapHandle(this) };
    EosMethodReturnValue(Connection::Constructor()->GetFunction()->NewInstance(1, argv));
}

NAN_METHOD(Eos::Environment::DataSources) {
    EOS_DEBUG_METHOD();

    SQLUSMALLINT direction = SQL_FETCH_FIRST;

    auto results = NanNew<Array>();

    if (args.Length() > 0) {
        Handle<String> type = args[0]->ToString();
        if (type->Equals(NanNew<String>("user")))
            direction = SQL_FETCH_FIRST_USER;
        else if (type->Equals(NanNew<String>("system")))
            direction = SQL_FETCH_FIRST_SYSTEM;
        else
            return NanThrowError("The first argument must be 'system', 'user', or omitted");
    }

    uint32_t i = 0;
    SQLRETURN ret;

    SQLWCHAR serverName[SQL_MAX_DSN_LENGTH + 1], description[256 + 1];
    SQLSMALLINT serverNameLength, descriptionLength;

    for(;;) {
        ret = SQLDataSourcesW(
            GetHandle(),
            direction,
            serverName, sizeof(serverName) / sizeof(SQLWCHAR), &serverNameLength,
            description, sizeof(description) / sizeof(SQLWCHAR), &descriptionLength);
    
        if (ret == SQL_NO_DATA)
            EosMethodReturnValue(results);

        if (ret == SQL_ERROR)
            return NanThrowError(GetLastError());

        auto item = NanNew<Object>();
        item->Set(NanSymbol("server"), StringFromTChar(serverName, min(sizeof(serverName) / sizeof(SQLWCHAR), (size_t)serverNameLength)));
        item->Set(NanSymbol("description"), StringFromTChar(description, min(sizeof(description) / sizeof(SQLWCHAR), (size_t)descriptionLength)));
        results->Set(i++, item);

        direction = SQL_FETCH_NEXT;
    }
}

NAN_METHOD(Eos::Environment::Drivers) {
    EOS_DEBUG_METHOD();

    SQLUSMALLINT direction = SQL_FETCH_FIRST;

    auto results = NanNew<Array>();

    uint32_t i = 0;
    SQLRETURN ret;

    const int maxDescriptionLength = 1024, maxAttributesLength = 2048;
    SQLWCHAR description[maxDescriptionLength + 1], driverAttributes[maxAttributesLength + 1];
    SQLSMALLINT descriptionLength, driverAttributesLength;

    for(;;) {
        ret = SQLDriversW(
            GetHandle(),
            direction,
            description, sizeof(description) / sizeof(SQLWCHAR), &descriptionLength,
            driverAttributes, sizeof(driverAttributes) / sizeof(SQLWCHAR), &driverAttributesLength);
    
        if (ret == SQL_NO_DATA)
            EosMethodReturnValue(results);

        if (ret == SQL_ERROR)
            return NanThrowError(GetLastError());

        auto item = NanNew<Object>();
        item->Set(NanSymbol("name"), StringFromTChar(description, min(sizeof(description) / sizeof(SQLWCHAR), (size_t)descriptionLength)));

        auto attributes = NanNew<Array>();
        
        SQLWCHAR* str = driverAttributes;
        for (uint32_t j = 0;; j++) {
            auto attribute = StringFromTChar(str);
            if (attribute->Length() == 0)
                break;

            attributes->Set(j, attribute);
            str += sqlwcslen(str) + 1;
        }

        item->Set(NanSymbol("attributes"), attributes);
        results->Set(i++, item);

        direction = SQL_FETCH_NEXT;
    }
}

Eos::Environment::~Environment() {
    EOS_DEBUG_METHOD();
}

Persistent<FunctionTemplate> Eos::Environment::constructor_;

namespace { ClassInitializer<Eos::Environment> x; }
