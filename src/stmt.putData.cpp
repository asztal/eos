#include "stmt.hpp"
#include "parameter.hpp"
#include "buffer.hpp"

using namespace Eos;
using namespace Buffers;

namespace Eos {
    struct PutDataOperation : Operation<Statement, PutDataOperation> {
        PutDataOperation::PutDataOperation() {
            EOS_DEBUG_METHOD();
        }

        static Handle<Value> New(Statement* owner, const Arguments& args) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 3)
                return ThrowError("Too few arguments");

            auto value = args[1];
            auto bufferObject = args[2];

            SQLPOINTER buffer = nullptr;
            SQLLEN length = 0;

            if (JSBuffer::HasInstance(bufferObject)) {
                if(auto msg = JSBuffer::Unwrap(bufferObject, buffer, length))
                    return ThrowTypeError(msg);
            } else {

            }

            if (!value->IsUndefined()) {
                // TODO: How do I determine what the type of the parameter I am sending is?
                // 1. Instead of taking a value and a buffer, take a Parameter object
                //   * For DAE input-only parameters, it will require allocating a buffer
                //     if there isn't one.
                //   * For DAE input/output parameters, it may require allocating a buffer.
                //   * Add a prm.setValue(...) method to fill the buffer
                FillInputBuffer(

            } else {
                // The value is already in the buffer, and 
                // the buffer is appropriately sliced.
            }

            (new PutDataOperation())->Wrap(args.Holder());
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
        Persistent<Object> bufferObject_;
        SQLPOINTER buffer_;
        SQLLEN bufferLength_;
        SQLINTEGER indicator_;
    };
}

Handle<Value> Statement::PutData(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 3)
        return ThrowError("Statement::PutData() requires a value, a buffer, and a callback");

    Handle<Value> argv[] = { handle_, args[0], args[1], args[2] };

    return Begin<PutDataOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Statement, PutDataOperation>::constructor_;
namespace { ClassInitializer<PutDataOperation> ci; }
