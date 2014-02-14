#include "stmt.hpp"

using namespace Eos;

namespace Eos {
    struct ExecDirectOperation : Operation<Statement, ExecDirectOperation> {
        ExecDirectOperation::ExecDirectOperation(Handle<Value> sql)
            : sql_(sql)
        {
            EOS_DEBUG_METHOD();
            EOS_DEBUG(L"execDirect: %s\n", *sql_);
        }

        static Handle<Value> New(Statement* owner, const Arguments& args) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 3)
                return ThrowError("Too few arguments");

            if (!args[1]->IsString())
                return ThrowTypeError("Statement SQL should be a string");

            (new ExecDirectOperation(args[1]))->Wrap(args.Holder());
            return args.Holder();
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

            if (!SQL_SUCCEEDED(ret) && ret != SQL_NEED_DATA)
                return CallbackErrorOverride(ret);

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[] = { 
                Undefined(),
                ret == SQL_NEED_DATA ? True() : False()
            };

            EOS_DEBUG(L"Result: %i\n", ret);
            
            GetCallback()->Call(Context::GetCurrent()->Global(), 2, argv);
        }

    protected:
        WStringValue sql_;
    };
}

Handle<Value> Statement::ExecDirect(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 2)
        return ThrowError("Statement::ExecDirect() requires 2 arguments");

    Handle<Value> argv[] = { handle_, args[0], args[1] };
    return Begin<ExecDirectOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Statement, ExecDirectOperation>::constructor_;
namespace { ClassInitializer<ExecDirectOperation> ci; }