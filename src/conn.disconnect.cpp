#include "conn.hpp"

using namespace Eos;

namespace Eos {
    struct DisconnectOperation: Operation<Connection, DisconnectOperation> {
        static const char* Name() { return "DisconnectOperation"; }

        static Handle<Value> New(Connection* owner, const Arguments& args) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 2)
                return ThrowError("Too few arguments");

            (new DisconnectOperation())->Wrap(args.Holder());
            return args.Holder();
        }

        SQLRETURN CallOverride() {
            return SQLDisconnect(
                Owner()->GetHandle());
        }
    };
}

Handle<Value> Connection::Disconnect(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 1)
        return ThrowError("Connection::Disconnect() requires a callback");

    Handle<Value> argv[] = { handle_, args[0] };
    return Begin<DisconnectOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Connection, DisconnectOperation>::constructor_;
namespace { ClassInitializer<DisconnectOperation> ci; }
