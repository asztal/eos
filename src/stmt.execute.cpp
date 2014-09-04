#include "stmt.hpp"

using namespace Eos;

namespace Eos {
    struct ExecuteOperation : Operation<Statement, ExecuteOperation> {
        ExecuteOperation() {
            EOS_DEBUG_METHOD();
        }

        static EOS_OPERATION_CONSTRUCTOR(New, Statement) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 2)
                return NanError("Too few arguments");

            (new ExecuteOperation())->Wrap(args.Holder());

            EOS_OPERATION_CONSTRUCTOR_RETURN();
        }

        static const char* Name() { return "ExecuteOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            return SQLExecute(Owner()->GetHandle());
        }

        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            if (!SQL_SUCCEEDED(ret) && ret != SQL_NEED_DATA && ret != SQL_PARAM_DATA_AVAILABLE)
                return CallbackErrorOverride(ret);

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[] = { 
                NanUndefined(),
                ret == SQL_NEED_DATA ? NanTrue() : NanFalse(),
                ret == SQL_PARAM_DATA_AVAILABLE ? NanTrue() : NanFalse()
            };
            
            MakeCallback(argv);
        }
    };
}

NAN_METHOD(Statement::Execute) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 1)
        return NanThrowError("Statement::Execute() requires a callback");
    
    Handle<Value> argv[] = { NanObjectWrapHandle(this), args[0] };
    return Begin<ExecuteOperation>(argv);
}

template<> Persistent<FunctionTemplate> Operation<Statement, ExecuteOperation>::constructor_;
namespace { ClassInitializer<ExecuteOperation> ci; }
