#include "stmt.hpp"

using namespace Eos;

namespace Eos {
    struct PrepareOperation : Operation<Statement, PrepareOperation> {
        PrepareOperation::PrepareOperation(Handle<Value> sql)
            : sql_(sql)
        {
            EOS_DEBUG_METHOD_FMT(L"sql = %ls", *sql_);
        }

        static Handle<Value> New(Statement* owner, const Arguments& args) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 3)
                return ThrowError("Too few arguments");

            if (!args[1]->IsString())
                return ThrowTypeError("Statement SQL should be a string");

            (new PrepareOperation(args[1]))->Wrap(args.Holder());
            return args.Holder();
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

Handle<Value> Statement::Prepare(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 2)
        return ThrowError("Statement::Prepare() requires 2 arguments");

    Handle<Value> argv[] = { handle_, args[0], args[1] };
    return Begin<PrepareOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Statement, PrepareOperation>::constructor_;
namespace { ClassInitializer<PrepareOperation> ci; }
