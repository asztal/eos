#include "stmt.hpp"

using namespace Eos;

namespace Eos {
    struct MoreResultsOperation : Operation<Statement, MoreResultsOperation> {
        MoreResultsOperation::MoreResultsOperation() {
            EOS_DEBUG_METHOD();
        }

        static Handle<Value> New(Statement* owner, const Arguments& args) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 1)
                return ThrowError("Too few arguments");

            (new MoreResultsOperation())->Wrap(args.Holder());
            return args.Holder();
        }

        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            if (!SQL_SUCCEEDED(ret) && ret != SQL_NO_DATA && ret != SQL_PARAM_DATA_AVAILABLE)
                return CallbackErrorOverride(ret);

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[] = { 
                Undefined(),
                Boolean::New(ret != SQL_NO_DATA),
                Boolean::New(ret == SQL_PARAM_DATA_AVAILABLE)
            };
            
            GetCallback()->Call(Context::GetCurrent()->Global(), 2, argv);
        }

        static const char* Name() { return "MoreResultsOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            return SQLFetch(
                Owner()->GetHandle());
        }
    };
}

Handle<Value> Statement::MoreResults(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 1)
        return ThrowError("Statement::MoreResults() requires a callback");

    Handle<Value> argv[] = { handle_, args[0] };
    return Begin<MoreResultsOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Statement, MoreResultsOperation>::constructor_;
namespace { ClassInitializer<MoreResultsOperation> ci; }
