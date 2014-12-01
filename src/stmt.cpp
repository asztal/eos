#include "stmt.hpp"
#include "parameter.hpp"
#include "buffer.hpp"

using namespace Eos;

Persistent<FunctionTemplate> Statement::constructor_;

void Statement::Init(Handle<Object> exports) {
    EOS_DEBUG_METHOD();

    EosHandle::Init("Statement", constructor_, New);
    
    auto sig0 = NanNew<Signature>(Constructor());
    EOS_SET_METHOD(Constructor(), "prepare", Statement, Prepare, sig0);
    EOS_SET_METHOD(Constructor(), "execDirect", Statement, ExecDirect, sig0);
    EOS_SET_METHOD(Constructor(), "execute", Statement, Execute, sig0);
    EOS_SET_METHOD(Constructor(), "fetch", Statement, Fetch, sig0);
    EOS_SET_METHOD(Constructor(), "getData", Statement, GetData, sig0);
    EOS_SET_METHOD(Constructor(), "cancel", Statement, Cancel, sig0);
    EOS_SET_METHOD(Constructor(), "numResultCols", Statement, NumResultCols, sig0);
    EOS_SET_METHOD(Constructor(), "describeCol", Statement, DescribeCol, sig0);
    EOS_SET_METHOD(Constructor(), "paramData", Statement, ParamData, sig0);
    EOS_SET_METHOD(Constructor(), "putData", Statement, PutData, sig0);
    EOS_SET_METHOD(Constructor(), "moreResults", Statement, MoreResults, sig0);
    EOS_SET_METHOD(Constructor(), "bindParameter", Statement, BindParameter, sig0);
    EOS_SET_METHOD(Constructor(), "setParameterName", Statement, SetParameterName, sig0);
    EOS_SET_METHOD(Constructor(), "unbindParameters", Statement, UnbindParameters, sig0);
    EOS_SET_METHOD(Constructor(), "bindCol", Statement, BindCol, sig0);
    EOS_SET_METHOD(Constructor(), "unbindColumn", Statement, UnbindColumn, sig0);
    EOS_SET_METHOD(Constructor(), "unbindColumns", Statement, UnbindColumns, sig0);
    EOS_SET_METHOD(Constructor(), "closeCursor", Statement, CloseCursor, sig0);
}

NAN_METHOD(Statement::New) {
    EOS_DEBUG_METHOD();

    NanScope();

    if (args.Length() < 1) {
        return NanThrowError("Statement::New() requires at least one argument");
    }

    if (!args.IsConstructCall()) {
        Handle<Value> argv[1] = { args[0] };
        NanReturnValue(Constructor()->GetFunction()->NewInstance(1, argv));
    }

    if (!Connection::Constructor()->HasInstance(args[0]))
        return NanThrowTypeError("The first argument to Statement::New() must be a Connection");

    auto conn = ObjectWrap::Unwrap<Connection>(args[0]->ToObject());
    
    SQLHSTMT hStmt;

    auto ret = SQLAllocHandle(SQL_HANDLE_STMT, conn->GetHandle(), &hStmt);
    if (!SQL_SUCCEEDED(ret))
        return NanThrowError(conn->GetLastError());

#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
    HANDLE hEvent = nullptr;

    ret = SQLSetStmtAttrW(
        hStmt,
        SQL_ATTR_ASYNC_ENABLE,
        (SQLPOINTER)SQL_ASYNC_ENABLE_ON,
        SQL_IS_INTEGER);

    if (SQL_SUCCEEDED(ret)) {
        hEvent = CreateEventW(nullptr, false, false, nullptr);
        if (!hEvent) {
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return NanThrowError("Unable to create wait handle");
        }

        ret = SQLSetStmtAttrW(
            hStmt,
            SQL_ATTR_ASYNC_STMT_EVENT,
            hEvent,
            SQL_IS_POINTER);

        if (!SQL_SUCCEEDED(ret)) {
            auto error = Eos::GetLastError(SQL_HANDLE_STMT, hStmt);
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            CloseHandle(hEvent);
            return NanThrowError(error);
        }
    }
#endif

    (new Statement(hStmt, conn EOS_ASYNC_ONLY_ARG(hEvent)))->Wrap(args.Holder());
    
    NanReturnValue(args.Holder());
}

Statement::Statement(SQLHSTMT hStmt, Connection* conn EOS_ASYNC_ONLY_ARG(HANDLE hEvent)) 
    : EosHandle(SQL_HANDLE_STMT, hStmt EOS_ASYNC_ONLY_ARG(hEvent))
    , connection_(conn)
{
    EOS_DEBUG_METHOD();
}

NAN_METHOD(Statement::Cancel) {
    EOS_DEBUG_METHOD();

    if(!SQL_SUCCEEDED(SQLCancelHandle(SQL_HANDLE_STMT, GetHandle())))
        return NanThrowError(GetLastError());

    NanReturnUndefined();
}

NAN_METHOD(Statement::BindParameter) {
    EOS_DEBUG_METHOD();
    
    if (args.Length() < 5)
        return NanThrowError("BindParameter expects 5, 6, or 7 arguments");
    
    if (!args[0]->IsInt32())
        return NanThrowTypeError("The 1st argument should be an integer");

    if (!args[1]->IsInt32())
        return NanThrowTypeError("The 2nd argument should be an integer");

    if (!args[2]->IsInt32())
        return NanThrowTypeError("The 3rd argument should be an integer");

    // 0. parameter number (e.g. 2)
    // 1. parameter kind (e.g. SQL_PARAM_INPUT)
    // 2. SQL type (e.g. SQL_INTEGER)
    // 3. decimal digits (e.g. 0)
    // 4. scale (e.g. 0)
    // 5. value (e.g. 27)
    // 6. buffer to use (e.g. new Buffer(4))

    auto parameterNumber = args[0]->Int32Value();
    if (parameterNumber < 1 || parameterNumber > USHRT_MAX)
        return NanThrowError("The parameter number is incorrect (valid values: 1 - 65535)");

    SQLSMALLINT inOutType = args[1]->Int32Value();
    SQLSMALLINT sqlType = args[2]->Int32Value();

    // max digits for SQL_DECIMAL, SQL_NUMERIC, SQL_FLOAT, SQL_REAL, or SQL_DOUBLE
    // max length for SQL_*CHAR, SQL_*BINARY
    SQLSMALLINT columnSize = args[3]->Int32Value();
    if (args[3]->Int32Value() > SHRT_MAX)
        return NanThrowRangeError("Column size is too high");

    // max digits after decimal point for SQL_NUMERIC/SQL_DECIMAL
    SQLSMALLINT decimalDigits = args[4]->Int32Value();
    if (auto j = args[4]->Int32Value() > SHRT_MAX)
        return NanThrowRangeError("Decimal digits is too high");

    Handle<Value> jsValue = NanUndefined();
    Handle<Object> bufferObject;

    if (args.Length() >= 6) 
        jsValue = args[5];

    if (args.Length() >= 7 && !args[6]->IsUndefined()) {
        if (!JSBuffer::HasInstance(args[6]) && !Buffer::HasInstance(args[6]))
            return NanThrowTypeError("The 7th argument should be a Buffer or SlowBuffer");

        bufferObject = args[6].As<Object>();
    }

    bool dae = jsValue->IsUndefined();

    Local<Object> jsParam;
    auto msg = Parameter::Marshal(parameterNumber, inOutType, sqlType, decimalDigits, jsValue, bufferObject, jsParam);
    if (msg) // Exception
        return NanThrowError(msg);

    auto param = Parameter::Unwrap(jsParam.As<Object>());

    // Setting the column length is only necessary for these types.
    if (!args[3]->IsInt32() 
        && !jsValue->IsUndefined() // Not data-at-execution
        && (sqlType == SQL_BINARY || sqlType == SQL_CHAR)) {

        if (param->Length() > SHRT_MAX)
            return NanThrowError("Parameter data is too big to be passed as SQL_BINARY or SQL_VARCHAR");

        columnSize = param->Length();
    }

    auto ret = SQLBindParameter(
        GetHandle(),
        parameterNumber,
        inOutType,
        param->CType(),
        param->SQLType(),
        columnSize,
        decimalDigits,
        param->Buffer(),
        param->Length(),
        &param->Indicator());

    if (!SQL_SUCCEEDED(ret))
        return NanThrowError(GetLastError());
  
    Statement::AddBoundParameter(param);

    EosMethodReturnValue(jsParam);
}

void Statement::AddBoundParameter(Parameter* param) {
    EOS_DEBUG_METHOD();

    if (boundParameters_.IsEmpty())
        NanAssignPersistent(boundParameters_, NanNew<Array>());

    NanNew(boundParameters_)->Set(param->ParameterNumber(), NanObjectWrapHandle(param));
}

NAN_METHOD(Statement::SetParameterName) {
    EOS_DEBUG_METHOD();

    if (args.Length() != 2)
        return NanThrowError("Statement::SetParameterName requires 2 arguments");

    if (!args[0]->IsInt32())
        return NanThrowError("The parameter number must be an integer");

    auto parameterNumber = args[0]->Int32Value();
    if (parameterNumber < 1 || parameterNumber >= USHRT_MAX)
        return NanThrowError("The parameter number must be between 1 and 65535 inclusive");

    WStringValue name(args[1]);

    SQLHDESC hIpd;

    auto ret = SQLGetStmtAttr(
        GetHandle(), 
        SQL_ATTR_IMP_PARAM_DESC, 
        &hIpd, 0, 0);

    if (!SQL_SUCCEEDED(ret))
        return NanThrowError(GetLastError());

    ret = SQLSetDescFieldW(
        hIpd, 
        parameterNumber, 
        SQL_DESC_NAME, 
        *name, 
        name.length() * sizeof(**name));

    if (!SQL_SUCCEEDED(ret))
        return NanThrowError(Eos::GetLastError(SQL_HANDLE_DESC, hIpd));

    NanReturnUndefined();
}

Parameter* Statement::GetBoundParameter(SQLUSMALLINT parameterNumber) {
    EOS_DEBUG_METHOD_FMT(L"%i", parameterNumber);

    if (boundParameters_.IsEmpty())
        return nullptr;

    auto handle = NanNew(boundParameters_)->Get(parameterNumber);
    if (handle->IsObject())
        return Parameter::Unwrap(handle.As<Object>());

    return nullptr;
}

NAN_METHOD(Statement::UnbindParameters) {
    EOS_DEBUG_METHOD();

    if(!SQL_SUCCEEDED(SQLFreeStmt(GetHandle(), SQL_RESET_PARAMS)))
        return NanThrowError(GetLastError());

	NanDisposePersistent(boundParameters_);

    NanReturnUndefined();
}

NAN_METHOD(Statement::BindCol) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 2)
        return NanThrowError("Too few arguments");

    if (!args[0]->IsInt32())
        return NanThrowTypeError("The column number must be an integer between 0 and 65535");

    auto columnNumber = args[0]->Int32Value();
    if (columnNumber < 0 || columnNumber > USHRT_MAX)
        return NanThrowRangeError("The column number must be an integer between 0 and 65535");
            
    if (!args[1]->IsInt32())
        return NanThrowTypeError("The column type must be an integer");
    auto type = args[1]->Int32Value();

    SQLPOINTER buffer;
    SQLLEN length;
    Handle<Object> jsBuffer;

    auto cType = Eos::GetCTypeForSQLType(type);
    auto requiredSize = Buffers::GetDesiredBufferLength(cType);
    
    if (args.Length() >= 3 && JSBuffer::HasInstance(args[2]))
        jsBuffer = args[2]->ToObject();
    else
        jsBuffer = JSBuffer::New(requiredSize > 0 ? requiredSize : 65536);

    assert(!jsBuffer.IsEmpty());

    if (auto msg = JSBuffer::Unwrap(jsBuffer, buffer, length))
        return NanThrowTypeError(msg);
    
    if (requiredSize && length < requiredSize)
        return NanThrowRangeError("The given buffer is not big enough to hold a value of the specified type.");

    assert(buffer && length);

    auto param = new(nothrow) Parameter(columnNumber, SQL_PARAM_OUTPUT, type, cType, buffer, length, jsBuffer, 0, true);
    if (!param)
        return NanThrowError("Out of memory allocating parameter");

    auto ret = SQLBindCol(
        GetHandle(),
        columnNumber,
        cType,
        buffer, length,
        &param->Indicator());

    EOS_DEBUG(L"Return value of SQLBindCol: %i\n", ret);

    if (!SQL_SUCCEEDED(ret))
        return NanThrowError(GetLastError());

	AddBoundColumn(param);

    EosMethodReturnValue(NanObjectWrapHandle(param));
}

void Statement::AddBoundColumn(Parameter* col) {
    EOS_DEBUG_METHOD();

    if (boundColumns_.IsEmpty())
        NanAssignPersistent(boundColumns_, NanNew<Array>());

    NanNew(boundColumns_)->Set(col->ParameterNumber(), NanObjectWrapHandle(col));
}

// TODO: Data-only option? (i.e. Unbind the data buffer, 
// but still receive the length/indicator value.)
NAN_METHOD(Statement::UnbindColumn) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 1)
        return NanThrowError("Too few arguments");

    if (!args[0]->IsInt32())
        return NanThrowTypeError("The column number must be an integer between 0 and 65535");

    auto columnNumber = args[0]->Int32Value();
    if (columnNumber < 0 || columnNumber > USHRT_MAX)
        return NanThrowRangeError("The column number must be an integer between 0 and 65535");

	auto ret = SQLBindCol(
		GetHandle(),
		columnNumber,
		SQL_DEFAULT,
		nullptr,
		0,
		nullptr);

    if (!SQL_SUCCEEDED(ret))
        return NanThrowError(GetLastError());

	if (!boundColumns_.IsEmpty()) {
		auto bc = NanNew(boundColumns_);
		bc->Delete(columnNumber);
		if (bc->Length() == 0)
			NanDisposePersistent(boundColumns_);
	} else {
		EOS_DEBUG(L"Couldn't remove bound column %d because boundColumns_.IsEmpty()\n", columnNumber);
	}

	// No need to dispose the Parameter here - JS-land might still have a reference to it.
	// It will be garbage collected if necessary.

    NanReturnUndefined();
}

NAN_METHOD(Statement::UnbindColumns) {
    EOS_DEBUG_METHOD();

    if(!SQL_SUCCEEDED(SQLFreeStmt(GetHandle(), SQL_UNBIND)))
        return NanThrowError(GetLastError());

	// No need to dispose the Parameters here. JS-land might still have a reference to them.
	// They'll be garbage collected if necessary.
	NanDisposePersistent(boundColumns_);

    NanReturnUndefined();
}

NAN_METHOD(Statement::CloseCursor) {
    EOS_DEBUG_METHOD();

    SQLRETURN ret;
    if (args.Length() > 0 && args[0]->IsTrue())
        ret = SQLCloseCursor(GetHandle()); // Can fail if no open cursor
    else
        ret = SQLFreeStmt(GetHandle(), SQL_CLOSE); // No error if no cursor

    if(!SQL_SUCCEEDED(ret))
        return NanThrowError(GetLastError());

    NanReturnUndefined();
}

void Statement::VirtualFree() {
    EOS_DEBUG_METHOD();

    if (!boundParameters_.IsEmpty())
        NanDisposePersistent(boundParameters_);

    if (!boundColumns_.IsEmpty())
        NanDisposePersistent(boundColumns_);
}

Statement::~Statement() {
    EOS_DEBUG_METHOD();
	
    VirtualFree();
}

#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)

// This shouldn't need to be called for a statement, since
// if a driver doesn't support asynchronous notifications then
// turning them on should fail
void Statement::DisableAsynchronousNotifications() {
    EOS_DEBUG_METHOD();

    auto ret = SQLSetConnectAttrW(
        GetHandle(),
        SQL_ATTR_ASYNC_STMT_EVENT,
        nullptr,
        SQL_IS_POINTER);

    if (!SQL_SUCCEEDED(ret))
        EOS_DEBUG(L"Failed to unset the connection's event handle");

    ret = SQLSetConnectAttrW(
        GetHandle(),
        SQL_ATTR_ASYNC_ENABLE,
        (SQLPOINTER)SQL_ASYNC_ENABLE_OFF,
        SQL_IS_INTEGER);

    if (!SQL_SUCCEEDED(ret))
        EOS_DEBUG(L"Failed to turn off asynchronous notifications");

    EosHandle::DisableAsynchronousNotifications();
}
#endif

namespace { ClassInitializer<Statement> c; }
