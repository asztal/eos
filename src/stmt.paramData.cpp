#include "stmt.hpp"
#include "parameter.hpp"

using namespace Eos;

namespace Eos {
    struct ParamDataOperation : Operation<Statement, ParamDataOperation> {
        ParamDataOperation::ParamDataOperation() {
            EOS_DEBUG_METHOD();
        }

        static Handle<Value> New(Statement* owner, const Arguments& args) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 1)
                return ThrowError("Too few arguments");

            (new ParamDataOperation())->Wrap(args.Holder());
            return args.Holder();
        }

        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            if (!SQL_SUCCEEDED(ret) && ret != SQL_NO_DATA && ret != SQL_PARAM_DATA_AVAILABLE && ret != SQL_NEED_DATA)
                return CallbackErrorOverride(ret);

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[] = { 
                Undefined(),
                Undefined(),
                Boolean::New(ret == SQL_NEED_DATA),
                Boolean::New(ret == SQL_PARAM_DATA_AVAILABLE)
            };

            if ((ret == SQL_PARAM_DATA_AVAILABLE || ret == SQL_NEED_DATA) && parameter_)
                argv[1] = reinterpret_cast<Parameter*>(parameter_)->handle_;
            
            Callback(argv);
        }

        static const char* Name() { return "ParamDataOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            return SQLParamData(Owner()->GetHandle(), &parameter_);
        }

    private:
        SQLPOINTER parameter_;
    };
}

Handle<Value> Statement::ParamData(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 1)
        return ThrowError("Statement::ParamData() requires a callback");

    Handle<Value> argv[] = { handle_, args[0] };
    return Begin<ParamDataOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Statement, ParamDataOperation>::constructor_;
namespace { ClassInitializer<ParamDataOperation> ci; }
