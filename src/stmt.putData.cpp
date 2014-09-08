#include "stmt.hpp"
#include "parameter.hpp"
#include "buffer.hpp"

using namespace Eos;
using namespace Buffers;

namespace Eos {
    struct PutDataOperation : Operation<Statement, PutDataOperation> {
        PutDataOperation(Parameter* param, Handle<Object> bufferObject, SQLPOINTER buffer, SQLLEN indicator)
            : parameter_(param)
            , buffer_(buffer)
            , indicator_(indicator)
        {
            EOS_DEBUG_METHOD();

            NanAssignPersistent(bufferObject_, bufferObject);
        }

        static EOS_OPERATION_CONSTRUCTOR(New, Statement) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 4)
                return NanError("Too few arguments");

            if (!Parameter::Constructor()->HasInstance(args[1]))
                return NanTypeError("The first parameter should be a Parameter");

            auto param = Parameter::Unwrap(args[1].As<Object>());
            Handle<Object> bufferObject;
            SQLPOINTER buffer = nullptr;
            SQLINTEGER indicator = 0;

            if (JSBuffer::HasInstance(args[2])) {
                bufferObject = args[2].As<Object>();

                if (auto msg = JSBuffer::Unwrap(bufferObject, buffer, indicator))
                    return NanError(msg);

                if (args[3]->IsNull())
                    indicator = SQL_NULL_DATA;
                else if (args[3]->IsNumber())
                    indicator = args[3]->IntegerValue();
            } else {
                bufferObject = param->BufferObject();
                buffer = param->Buffer();
                indicator = param->Indicator();

                if (indicator == SQL_DATA_AT_EXEC || bufferObject.IsEmpty())
                    return NanError("No buffer specified for putData and no buffer in the Parameter");
            }

            param->Ref();

            (new PutDataOperation(param, bufferObject, buffer, indicator))
                ->Wrap(args.Holder());

            EOS_OPERATION_CONSTRUCTOR_RETURN();
        }

        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            parameter_->Unref();

            if (!SQL_SUCCEEDED(ret) && ret != SQL_NO_DATA && ret != SQL_PARAM_DATA_AVAILABLE && ret != SQL_NEED_DATA)
                return CallbackErrorOverride(ret);

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[] = { 
                NanUndefined(),
                NanNew<Boolean>(ret == SQL_NEED_DATA),
                NanNew<Boolean>(ret == SQL_PARAM_DATA_AVAILABLE)
            };

            MakeCallback(argv);
        }

        static const char* Name() { return "PutDataOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            return SQLPutData(
                Owner()->GetHandle(),
                buffer_,
                indicator_);
        }

    private:
        Parameter* parameter_;
        SQLPOINTER buffer_;
        SQLLEN indicator_;
        Persistent<Object> bufferObject_;
    };
}

NAN_METHOD(Statement::PutData) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 2)
        return NanThrowError("Statement::PutData() requires a parameter, a buffer, a number of bytes, and a callback");

    Handle<Value> argv[] = { NanObjectWrapHandle(this), args[0], args[1], args[2], args[3] };

    return Begin<PutDataOperation>(argv);
}

template<> Persistent<FunctionTemplate> Operation<Statement, PutDataOperation>::constructor_ = Persistent<FunctionTemplate>();
namespace { ClassInitializer<PutDataOperation> ci; }
