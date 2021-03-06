#include "stmt.hpp"

using namespace Eos;

namespace Eos {
    struct NumResultColsOperation : Operation<Statement, NumResultColsOperation> {
        NumResultColsOperation() {
            EOS_DEBUG_METHOD();
        }

        static EOS_OPERATION_CONSTRUCTOR(New, Statement) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 1)
                return NanError("Too few arguments");

            (new NumResultColsOperation())->Wrap(args.Holder());

            EOS_OPERATION_CONSTRUCTOR_RETURN();
        }

        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            if (!SQL_SUCCEEDED(ret))
                return CallbackErrorOverride(ret);

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[] = { 
                NanUndefined(),
                NanNew<Integer>(columnCount_)
            };
            
            MakeCallback(argv);
        }

        static const char* Name() { return "NumResultColsOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            return SQLNumResultCols(
                Owner()->GetHandle(),
                &columnCount_);
        }

    private:
        SQLSMALLINT columnCount_;
    };
}

NAN_METHOD(Statement::NumResultCols) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 1)
        return NanThrowError("Statement::NumResultCols() requires a callback");

    Handle<Value> argv[] = { NanObjectWrapHandle(this), args[0] };
    return Begin<NumResultColsOperation>(argv);
}

template<> Persistent<FunctionTemplate> Eos::Operation<Statement, NumResultColsOperation>::constructor_ = Persistent<FunctionTemplate>();
namespace { ClassInitializer<NumResultColsOperation> ci; }
