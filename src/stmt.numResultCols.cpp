#include "stmt.hpp"

using namespace Eos;

namespace Eos {
    struct NumResultColsOperation : Operation<Statement, NumResultColsOperation> {
        NumResultColsOperation::NumResultColsOperation() {
            EOS_DEBUG_METHOD();
        }

        static Handle<Value> New(Statement* owner, const Arguments& args) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 1)
                return ThrowError("Too few arguments");

            (new NumResultColsOperation())->Wrap(args.Holder());
            return args.Holder();
        }

        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            if (!SQL_SUCCEEDED(ret))
                return CallbackErrorOverride(ret);

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[] = { 
                Undefined(),
                Integer::New(columnCount_)
            };
            
            GetCallback()->Call(Context::GetCurrent()->Global(), 2, argv);
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

Handle<Value> Statement::NumResultCols(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 1)
        return ThrowError("Statement::NumResultCols() requires a callback");

    Handle<Value> argv[] = { handle_, args[0] };
    return Begin<NumResultColsOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Statement, NumResultColsOperation>::constructor_;
namespace { ClassInitializer<NumResultColsOperation> ci; }