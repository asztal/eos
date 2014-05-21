#include "conn.hpp"

using namespace Eos;

namespace Eos {
    struct DriverConnectOperation : Operation<Connection, DriverConnectOperation> {
        DriverConnectOperation::DriverConnectOperation(Handle<Value> connectionString)
            : connectionString_(connectionString)
        {
            EOS_DEBUG_METHOD_FMT(L"%ls", *connectionString_);
        }

        static EOS_OPERATION_CONSTRUCTOR(New, Connection) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 3)
                return NanThrowError("Too few arguments");

            if (!args[1]->IsString())
                return NanThrowTypeError("Connection string should be a string");

            (new DriverConnectOperation(args[1]))->Wrap(args.Holder());
            NanReturnValue(args.Holder());
        }

        static const char* Name() { return "DriverConnectOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            return SQLDriverConnect(
                Owner()->GetHandle(), 
                SQL_NULL_HANDLE,
                *connectionString_, connectionString_.length(),
                nullptr, 0, &cchCompleted_,
                SQL_DRIVER_NOPROMPT);
        }

    protected:
        SQLSMALLINT cchCompleted_; // Not used
        WStringValue connectionString_;
    };
}

NAN_METHOD(Connection::DriverConnect) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 2)
        return NanThrowError("Connection::DriverConnect() requires 2 arguments");

    Handle<Value> argv[] = { handle(), args[0], args[1] };
    return Begin<DriverConnectOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Connection, DriverConnectOperation>::constructor_;
namespace { ClassInitializer<DriverConnectOperation> ci; }
