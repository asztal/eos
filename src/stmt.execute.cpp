#include "stmt.hpp"

using namespace Eos;

namespace Eos {
    struct ExecuteOperation : Operation<Statement, ExecuteOperation> {
        ExecuteOperation::ExecuteOperation() {
            EOS_DEBUG_METHOD();
        }

        static Handle<Value> New(Statement* owner, const Arguments& args) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 2)
                return ThrowError("Too few arguments");

            (new ExecuteOperation())->Wrap(args.Holder());
            return args.Holder();
        }

        static const char* Name() { return "ExecuteOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            return SQLExecute(Owner()->GetHandle());
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
            
            GetCallback()->Call(Context::GetCurrent()->Global(), 2, argv);
        }
    };
}

Handle<Value> Statement::Execute(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 1)
        return ThrowError("Statement::Execute() requires a callback");

    Handle<Value> argv[] = { handle_, args[0] };
    return Begin<ExecuteOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Statement, ExecuteOperation>::constructor_;
namespace { ClassInitializer<ExecuteOperation> ci; }
