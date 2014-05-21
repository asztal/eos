#include "stmt.hpp"

using namespace Eos;

namespace Eos {
    struct FetchOperation : Operation<Statement, FetchOperation> {
        FetchOperation::FetchOperation() {
            EOS_DEBUG_METHOD();
        }

        static EOS_OPERATION_CONSTRUCTOR(New, Statement) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 1)
                return NanError("Too few arguments");

            (new FetchOperation())->Wrap(args.Holder());

            EOS_OPERATION_CONSTRUCTOR_RETURN();
        }

        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            if (!SQL_SUCCEEDED(ret) && ret != SQL_NO_DATA)
                return CallbackErrorOverride(ret);

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[] = { 
                NanUndefined(),
                ret == SQL_NO_DATA ? NanFalse() : NanTrue()
            };
            
            Callback(argv);
        }

        static const char* Name() { return "FetchOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            return SQLFetch(
                Owner()->GetHandle());
        }
    };
}

NAN_METHOD(Statement::Fetch) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 1)
        return NanThrowError("Statement::Fetch() requires a callback");

    Handle<Value> argv[] = { NanObjectWrapHandle(this), args[0] };
    return Begin<FetchOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Statement, FetchOperation>::constructor_;
namespace { ClassInitializer<FetchOperation> ci; }