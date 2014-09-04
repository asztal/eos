#include "stmt.hpp"
#include "parameter.hpp"

using namespace Eos;

namespace Eos {
    struct ParamDataOperation : Operation<Statement, ParamDataOperation> {
        ParamDataOperation() 
            : parameter_(nullptr)
        {
            EOS_DEBUG_METHOD();
        }

        static EOS_OPERATION_CONSTRUCTOR(New, Statement) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 1)
                return NanError("Too few arguments");

            (new ParamDataOperation())->Wrap(args.Holder());

            EOS_OPERATION_CONSTRUCTOR_RETURN();
        }

        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            if (!SQL_SUCCEEDED(ret) && ret != SQL_NO_DATA && ret != SQL_PARAM_DATA_AVAILABLE && ret != SQL_NEED_DATA)
                return CallbackErrorOverride(ret);

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[] = { 
                NanUndefined(),
                NanUndefined(),
                NanNew<Boolean>(ret == SQL_NEED_DATA),
                NanNew<Boolean>(ret == SQL_PARAM_DATA_AVAILABLE)
            };

            if ((ret == SQL_PARAM_DATA_AVAILABLE || ret == SQL_NEED_DATA) && parameter_)
                argv[1] = NanObjectWrapHandle(reinterpret_cast<Parameter*>(parameter_));
            
            MakeCallback(argv);
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

NAN_METHOD(Statement::ParamData) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 1)
        return NanThrowError("Statement::ParamData() requires a callback");

    Handle<Value> argv[] = { NanObjectWrapHandle(this), args[0] };
    return Begin<ParamDataOperation>(argv);
}

template<> Persistent<FunctionTemplate> Operation<Statement, ParamDataOperation>::constructor_;
namespace { ClassInitializer<ParamDataOperation> ci; }
