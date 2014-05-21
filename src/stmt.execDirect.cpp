#include "stmt.hpp"

using namespace Eos;

namespace Eos {
    struct ExecDirectOperation : Operation<Statement, ExecDirectOperation> {
        ExecDirectOperation::ExecDirectOperation(Handle<Value> sql)
            : sql_(sql)
        {
            EOS_DEBUG_METHOD_FMT(L"execDirect: %ls\n", *sql_);
        }

        static EOS_OPERATION_CONSTRUCTOR(New, Statement) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 3)
                return NanError("Too few arguments");

            if (!args[1]->IsString())
                return NanTypeError("Statement SQL should be a string");

            (new ExecDirectOperation(args[1]))->Wrap(args.Holder());

            EOS_OPERATION_CONSTRUCTOR_RETURN();
        }

        static const char* Name() { return "ExecDirectOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            return SQLExecDirectW(
                Owner()->GetHandle(), 
                *sql_, sql_.length());
        }

        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            if (!SQL_SUCCEEDED(ret) && ret != SQL_NEED_DATA && ret != SQL_PARAM_DATA_AVAILABLE)
                return CallbackErrorOverride(ret);

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[] = { 
                NanUndefined(),
                ret == SQL_NEED_DATA ? NanTrue() : NanFalse(),
                ret == SQL_PARAM_DATA_AVAILABLE ? NanTrue() : NanFalse(),
            };
            
            Callback(argv);
        }

    protected:
        WStringValue sql_;
    };
}

NAN_METHOD(Statement::ExecDirect) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 2)
        return NanThrowError("Statement::ExecDirect() requires 2 arguments");

    Handle<Value> argv[] = { NanObjectWrapHandle(this), args[0], args[1] };
    return Begin<ExecDirectOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Statement, ExecDirectOperation>::constructor_;
namespace { ClassInitializer<ExecDirectOperation> ci; }