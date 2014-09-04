#include "stmt.hpp"

using namespace Eos;

namespace Eos {
    struct MoreResultsOperation : Operation<Statement, MoreResultsOperation> {
        MoreResultsOperation() {
            EOS_DEBUG_METHOD();
        }

        static EOS_OPERATION_CONSTRUCTOR(New, Statement) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 1)
                return NanError("Too few arguments");

            (new MoreResultsOperation())->Wrap(args.Holder());

            EOS_OPERATION_CONSTRUCTOR_RETURN();
        }

        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            if (!SQL_SUCCEEDED(ret) && ret != SQL_NO_DATA && ret != SQL_PARAM_DATA_AVAILABLE)
                return CallbackErrorOverride(ret);

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[] = { 
                NanUndefined(),
                NanNew<Boolean>(ret != SQL_NO_DATA),
                NanNew<Boolean>(ret == SQL_PARAM_DATA_AVAILABLE)
            };
            
            MakeCallback(argv);
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

NAN_METHOD(Statement::MoreResults) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 1)
        return NanThrowError("Statement::MoreResults() requires a callback");

    Handle<Value> argv[] = { NanObjectWrapHandle(this), args[0] };
    return Begin<MoreResultsOperation>(argv);
}

template<> Persistent<FunctionTemplate> Operation<Statement, MoreResultsOperation>::constructor_;
namespace { ClassInitializer<MoreResultsOperation> ci; }
