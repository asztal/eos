#include "conn.hpp"

using namespace Eos;

namespace Eos {
    struct DisconnectOperation: Operation<Connection, DisconnectOperation> {
        static const char* Name() { return "DisconnectOperation"; }

        static EOS_OPERATION_CONSTRUCTOR(New, Connection) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 2)
                return NanError("Too few arguments");

            (new DisconnectOperation())->Wrap(args.Holder());

            EOS_OPERATION_CONSTRUCTOR_RETURN();
        }

        SQLRETURN CallOverride() {
            return SQLDisconnect(
                Owner()->GetHandle());
        }
    };
}

NAN_METHOD(Connection::Disconnect) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 1)
        return NanThrowError("Connection::Disconnect() requires a callback");

    Handle<Value> argv[] = { NanObjectWrapHandle(this), args[0] };
    return Begin<DisconnectOperation>(argv);
}

template<> Persistent<FunctionTemplate> Operation<Connection, DisconnectOperation>::constructor_ = Persistent<FunctionTemplate>();
namespace { ClassInitializer<DisconnectOperation> ci; }
