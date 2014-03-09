#include "stmt.hpp"
#include <ctime>
#include <climits>

using namespace Eos;

namespace Eos {
    struct GetDataOperation : Operation<Statement, GetDataOperation> {
        GetDataOperation::GetDataOperation
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
            , bufferHandle_(Persistent<Object>::New(bufferHandle))
            , bufferLength_(bufferLength)
            , totalLength_(0)
            , raw_(raw)
        {
            EOS_DEBUG_METHOD_FMT(L"%hu, type = %hi", columnNumber_, sqlType_);

            cType_ = GetCTypeForSQLType(sqlType_);

            if (buffer_ == nullptr) {
                if (cType_ == SQL_C_BINARY 
                    || cType_ == SQL_C_WCHAR 
                    || cType_ == SQL_C_CHAR
                    || raw_)
                {
                    bufferHandle_ = Persistent<Object>::New(JSBuffer::New(65536));
                
                    auto msg = JSBuffer::Unwrap(bufferHandle_, buffer_, bufferLength_);
                    assert(msg == nullptr);
                } else {
                    buffer_ = &rawValues_;
                    bufferLength_ = sizeof(rawValues_);
                }
            }
        }

        static Handle<Value> New(Statement* owner, const Arguments& args) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 6)
                return ThrowError("Too few arguments");

            auto columnNumber = args[1]->Int32Value();
            auto sqlType = args[2]->Int32Value();

            if (!args[1]->IsUint32() || columnNumber > USHRT_MAX)
                return ThrowError("Column number must be an integer from 0 to 65535");

            if (!args[2]->IsInt32() || sqlType < SHRT_MIN || sqlType > SHRT_MAX)
                return ThrowError("Target type is out of range");

            Handle<Object> bufferHandle;
            SQLPOINTER buffer;
            SQLLEN bufferLength;
            bool raw;
            bool ownBuffer;

            if (args[3]->IsObject() && args[3].As<Object>()->GetConstructor()->Equals(JSBuffer::Constructor())) {
                bufferHandle = args[3].As<Object>();
                
                auto msg = JSBuffer::Unwrap(bufferHandle, buffer, bufferLength);
                if (msg)
                    return ThrowError(msg);
            } else if (args[3]->IsObject() && Buffer::constructor_template->HasInstance(args[3])) {
                bufferHandle = args[3].As<Object>();
                auto slow = ObjectWrap::Unwrap<Buffer>(bufferHandle);
                buffer = Buffer::Data(slow);
                bufferLength = Buffer::Length(slow);
                ownBuffer = false;
            } else {
                if (!args[3]->IsUndefined() && !args[3]->IsNull())
                    return ThrowError("Unknown buffer type (pass null to have a buffer created automatically)");

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
            return args.Holder();
        }

        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            if (!SQL_SUCCEEDED(ret) && ret != SQL_NO_DATA)
                return CallbackErrorOverride(ret);

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[4];
            argv[0] = Undefined();
            argv[2] = (totalLength_ != SQL_NO_TOTAL)
                    ? Int32::New(totalLength_)
                    : Undefined();
            argv[3] = Boolean::New(totalLength_ > bufferLength_ || (totalLength_ == SQL_NO_TOTAL && ret == SQL_SUCCESS_WITH_INFO));

            if (ret == SQL_NO_DATA)
                argv[1] = Undefined();
            else if (totalLength_ == SQL_NULL_DATA)
                argv[1] = Null();
            else if (raw_) {
                assert(!bufferHandle_.IsEmpty());
                argv[1] = bufferHandle_;
            } else if (cType_ == SQL_C_BINARY) {
                if (totalLength_ >= bufferLength_) 
                    argv[1] = bufferHandle_;
                else
                    argv[1] = JSBuffer::Slice(bufferHandle_, 0, totalLength_);
            } else {
                argv[1] = Eos::ConvertToJS(buffer_, totalLength_, bufferLength_, cType_);
                if (argv[1]->IsUndefined())
                    argv[0] = OdbcError("Unable to interpret contents of result buffer");
            }
            
            Callback(argv);
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
            long l;
            bool b;
            double d;
            tm tm;
            SQL_TIMESTAMP_STRUCT ts;
        } rawValues_;

        bool raw_;
    };
}

Handle<Value> Statement::GetData(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 5)
        return ThrowError("Statement::GetData() requires 4 arguments and a callback");

    Handle<Value> argv[] = { handle_, args[0], args[1], args[2], args[3], args[4] };
    return Begin<GetDataOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Statement, GetDataOperation>::constructor_;
namespace { ClassInitializer<GetDataOperation> ci; }