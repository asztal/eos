#include "stmt.hpp"
#include <limits>

using namespace Eos;

namespace Eos {
    struct DescribeColOperation : Operation<Statement, DescribeColOperation> {
        DescribeColOperation::DescribeColOperation(SQLUSMALLINT columnNumber) 
            : columnNumber_(columnNumber)
        {
            EOS_DEBUG_METHOD();
        }

        static EOS_OPERATION_CONSTRUCTOR(New, Statement) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 2)
                return NanError("Too few arguments");

            if (!args[1]->IsInt32())
                return NanTypeError("The column number must be an integer between 0 and 65535");

            auto columnNumber = args[1]->Int32Value();
            if (columnNumber < 0 || columnNumber > USHRT_MAX)
                return NanRangeError("The column number must be an integer between 0 and 65535");

            (new DescribeColOperation(columnNumber))->Wrap(args.Holder());

            EOS_OPERATION_CONSTRUCTOR_RETURN();
        }

        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            if (!SQL_SUCCEEDED(ret))
                return CallbackErrorOverride(ret);

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[] = { 
                NanUndefined(),
                StringFromTChar(columnName_, min<SQLSMALLINT>(maxColumnNameLength, columnNameLength_)),
                NanNew<Integer>(dataType_),
                NanNew<Integer>(columnSize_),
                NanNew<Integer>(decimalDigits_),
                nullable_ == SQL_NULLABLE_UNKNOWN 
                    ? NanUndefined()
                    : (nullable_ == SQL_NULLABLE ? NanTrue() : NanFalse())
            };
            
            MakeCallback(argv);
        }

        static const char* Name() { return "NumResultColsOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            return SQLDescribeColW(
                Owner()->GetHandle(),
                columnNumber_,
                columnName_, sizeof(columnName_) / sizeof(SQLWCHAR), &columnNameLength_,
                &dataType_,
                &columnSize_,
                &decimalDigits_,
                &nullable_);
        }

    private:
        SQLUSMALLINT columnNumber_;
        enum { maxColumnNameLength = 1025 };
        SQLWCHAR columnName_[maxColumnNameLength];
        SQLSMALLINT columnNameLength_;
        SQLSMALLINT dataType_;
        SQLULEN columnSize_;
        SQLSMALLINT decimalDigits_;
        SQLSMALLINT nullable_;
    };
}

NAN_METHOD(Statement::DescribeCol) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 2)
        return NanThrowError(
            "Statement::DescribeCol() requires a column number"
            "(starting from 1, or 0 for the bookmark column) a callback");

    Handle<Value> argv[] = { NanObjectWrapHandle(this), args[0], args[1] };
    return Begin<DescribeColOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Statement, DescribeColOperation>::constructor_;
namespace { ClassInitializer<DescribeColOperation> ci; }