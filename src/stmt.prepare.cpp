#include "stmt.hpp"

using namespace Eos;

namespace Eos {
    struct PrepareOperation : Operation<Statement, PrepareOperation> {
        PrepareOperation(Handle<Value> sql)
            : sql_(sql)
        {
            EOS_DEBUG_METHOD_FMT(L"sql = %ls", *sql_);
        }

        static EOS_OPERATION_CONSTRUCTOR(New, Statement) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 3)
                return NanError("Too few arguments");

            if (!args[1]->IsString())
                return NanTypeError("Statement SQL should be a string");

            (new PrepareOperation(args[1]))->Wrap(args.Holder());

            EOS_OPERATION_CONSTRUCTOR_RETURN();
        }

        static const char* Name() { return "PrepareOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            return SQLPrepareW(
                Owner()->GetHandle(), 
                *sql_, sql_.length());
        }

    protected:
        WStringValue sql_;
    };
}

NAN_METHOD(Statement::Prepare) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 2)
        return NanThrowError("Statement::Prepare() requires 2 arguments");

    Handle<Value> argv[] = { NanObjectWrapHandle(this), args[0], args[1] };
    return Begin<PrepareOperation>(argv);
}

template<> Persistent<FunctionTemplate> Operation<Statement, PrepareOperation>::constructor_ = Persistent<FunctionTemplate>();
namespace { ClassInitializer<PrepareOperation> ci; }
