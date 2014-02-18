#include "conn.hpp"

using namespace Eos;

namespace Eos {
    struct ConnectOperation : Operation<Connection, ConnectOperation> {
        ConnectOperation::ConnectOperation(Handle<Value> connectionString)
            : connectionString_(connectionString)
        {
            EOS_DEBUG_METHOD_FMT(L"%ls", *connectionString_);
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

    protected:
        WStringValue connectionString_;
    };
}

Handle<Value> Connection::Connect(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 2)
        return ThrowError("Connection::Connect() requires 2 arguments");

    Handle<Value> argv[] = { handle_, args[0], args[1] };
    return Begin<ConnectOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Connection, ConnectOperation>::constructor_;
namespace { ClassInitializer<ConnectOperation> ci; }
