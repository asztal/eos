#include "stmt.hpp"

using namespace Eos;

namespace Eos {
    struct PrepareOperation : Operation<Statement, PrepareOperation> {
        PrepareOperation::PrepareOperation(Handle<Value> sql)
            : sql_(sql)
        {
            EOS_DEBUG_METHOD_FMT(L"sql = %ls", *sql_);
        }

        static EOS_OPERATION_CONSTRUCTOR(New, Statement) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 3)
                return NanThrowError("Too few arguments");

            if (!args[1]->IsString())
                return NanThrowTypeError("Statement SQL should be a string");

            (new PrepareOperation(args[1]))->Wrap(args.Holder());
            NanReturnValue(args.Holder());
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

Persistent<FunctionTemplate> Operation<Statement, PrepareOperation>::constructor_;
namespace { ClassInitializer<PrepareOperation> ci; }
