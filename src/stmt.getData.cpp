#include "stmt.hpp"
#include <ctime>
#include <climits>

using namespace Eos;

namespace Eos {
    struct GetDataOperation : Operation<Statement, GetDataOperation> {
        GetDataOperation
            ( SQLUSMALLINT columnNumber
            , SQLSMALLINT sqlType
            , SQLPOINTER buffer
            , SQLLEN bufferLength
            , Handle<Object> bufferHandle
            , bool raw
            )
            : columnNumber_(columnNumber)
            , sqlType_(sqlType)
            , buffer_(buffer)
            , bufferLength_(bufferLength)
            , totalLength_(0)
            , raw_(raw)
        {
            EOS_DEBUG_METHOD_FMT(L"%hu, type = %hi", columnNumber_, sqlType_);

            NanAssignPersistent(bufferHandle_, bufferHandle);

            cType_ = GetCTypeForSQLType(sqlType_);

            if (buffer_ == nullptr) {
                assert(bufferHandle_.IsEmpty());

                if (cType_ == SQL_C_BINARY 
                    || cType_ == SQL_C_WCHAR 
                    || cType_ == SQL_C_CHAR
                    || raw_)
                {
                    NanAssignPersistent(bufferHandle_, JSBuffer::New(65536));
                
                    auto msg = JSBuffer::Unwrap(NanNew(bufferHandle_), buffer_, bufferLength_);
                    assert(msg == nullptr);
                } else {
                    buffer_ = &rawValues_;
                    bufferLength_ = sizeof(rawValues_);
                }
            }
        }

        ~GetDataOperation() {
            EOS_DEBUG_METHOD();
            if (!bufferHandle_.IsEmpty()) {
                EOS_DEBUG(L"Warning! Buffer handle not released by GetDataOperation. (Memory held onto for longer than needed.)\n");
                NanDisposePersistent(bufferHandle_);
            }
        }

        static EOS_OPERATION_CONSTRUCTOR(New, Statement) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 6)
                return NanError("Too few arguments");

            auto columnNumber = args[1]->Int32Value();
            auto sqlType = args[2]->Int32Value();

            if (!args[1]->IsUint32() || columnNumber > USHRT_MAX)
                return NanError("Column number must be an integer from 0 to 65535");

            if (!args[2]->IsInt32() || sqlType < SHRT_MIN || sqlType > SHRT_MAX)
                return NanError("Target type is out of range");

            Handle<Object> bufferHandle;
            SQLPOINTER buffer;
            SQLLEN bufferLength;
            bool raw;
            bool ownBuffer;

            if (args[3]->IsObject() && args[3].As<Object>()->GetConstructor()->Equals(JSBuffer::Constructor())) {
                bufferHandle = args[3].As<Object>();
                
                auto msg = JSBuffer::Unwrap(bufferHandle, buffer, bufferLength);
                if (msg)
                    return NanError(msg);
            } else if (args[3]->IsObject() && Buffer::HasInstance(args[3])) {
                buffer = Buffer::Data(args[3]);
                bufferLength = Buffer::Length(args[3]);
                ownBuffer = false;
            } else {
                if (!args[3]->IsUndefined() && !args[3]->IsNull())
                    return NanError("Unknown buffer type (pass null to have a buffer created automatically)");

                // The operation can choose to allocate a buffer if it decides it necessary
                // based on targetType.
                buffer = nullptr;
                bufferLength = 0;
            }
            
            raw = args[4]->BooleanValue();

            (new GetDataOperation(
                columnNumber, 
                sqlType, 
                buffer, bufferLength, bufferHandle,
                raw))->Wrap(args.Holder());

            EOS_OPERATION_CONSTRUCTOR_RETURN();
        }

        // Any new return statements added here should call MakeWeak() on bufferHandle_,
        // if it is not empty.
        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            if (!SQL_SUCCEEDED(ret) && ret != SQL_NO_DATA) {
                if (!bufferHandle_.IsEmpty())
                    NanDisposePersistent(bufferHandle_);
                return CallbackErrorOverride(ret);
            }

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[4];

            argv[0] = NanUndefined();
            if (totalLength_ != SQL_NO_TOTAL)
                argv[2] = NanNew<Number>(totalLength_);
            else
                argv[2] = NanUndefined();
            argv[3] = NanNew<Boolean>(totalLength_ > bufferLength_ || (totalLength_ == SQL_NO_TOTAL && ret == SQL_SUCCESS_WITH_INFO));

            if (ret == SQL_NO_DATA)
                argv[1] = NanUndefined();
            else if (totalLength_ == SQL_NULL_DATA)
                argv[1] = NanNull();
            else if (raw_) {
                assert(!bufferHandle_.IsEmpty());
                argv[1] = NanNew(bufferHandle_);
            } else if (cType_ == SQL_C_BINARY) {
                if (totalLength_ >= bufferLength_) 
                    argv[1] = NanNew(bufferHandle_);
                else
                    argv[1] = JSBuffer::Slice(NanNew(bufferHandle_), 0, totalLength_);
            } else {
                argv[1] = Eos::ConvertToJS(buffer_, totalLength_, bufferLength_, cType_);
                if (argv[1]->IsUndefined())
                    argv[0] = OdbcError("Unable to interpret contents of result buffer");
            }

            // Can we Dispose() things that JS-land is referencing? We'll soon find out!
            if (!bufferHandle_.IsEmpty())
                NanDisposePersistent(bufferHandle_);
            
            MakeCallback(argv);
        }

        static const char* Name() { return "GetDataOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            return SQLGetData(
                Owner()->GetHandle(),
                columnNumber_,
                cType_,
                buffer_, bufferLength_, 
                &totalLength_);
        }

    private:
        SQLUSMALLINT columnNumber_;
        SQLSMALLINT sqlType_, cType_;
        SQLPOINTER buffer_;
        Persistent<Object> bufferHandle_;
        SQLLEN bufferLength_, totalLength_;

        union {
            bool b;
            double d;
            SQL_TIMESTAMP_STRUCT ts;
        } rawValues_;

        bool raw_;
    };
}

NAN_METHOD(Statement::GetData) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 5)
        return NanThrowError("Statement::GetData() requires 4 arguments and a callback");

    Handle<Value> argv[] = { NanObjectWrapHandle(this), args[0], args[1], args[2], args[3], args[4] };
    return Begin<GetDataOperation>(argv);
}

template<> Persistent<FunctionTemplate> Operation<Statement, GetDataOperation>::constructor_ = Persistent<FunctionTemplate>();
namespace { ClassInitializer<GetDataOperation> ci; }
