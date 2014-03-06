#include "stmt.hpp"
#include "parameter.hpp"

using namespace Eos;

Persistent<FunctionTemplate> Statement::constructor_;

void Statement::Init(Handle<Object> exports) {
    EOS_DEBUG_METHOD();

    EosHandle::Init("Statement", constructor_, New);
    
    auto sig0 = Signature::New(constructor_);
    EOS_SET_METHOD(constructor_, "prepare", Statement, Prepare, sig0);
    EOS_SET_METHOD(constructor_, "execDirect", Statement, ExecDirect, sig0);
    EOS_SET_METHOD(constructor_, "execute", Statement, Execute, sig0);
    EOS_SET_METHOD(constructor_, "fetch", Statement, Fetch, sig0);
    EOS_SET_METHOD(constructor_, "getData", Statement, GetData, sig0);
    EOS_SET_METHOD(constructor_, "cancel", Statement, Cancel, sig0);
    EOS_SET_METHOD(constructor_, "numResultCols", Statement, NumResultCols, sig0);
    EOS_SET_METHOD(constructor_, "describeCol", Statement, DescribeCol, sig0);
    EOS_SET_METHOD(constructor_, "moreResults", Statement, MoreResults, sig0);
    EOS_SET_METHOD(constructor_, "bindParameter", Statement, BindParameter, sig0);
    EOS_SET_METHOD(constructor_, "setParameterName", Statement, SetParameterName, sig0);
    EOS_SET_METHOD(constructor_, "unbindParameters", Statement, UnbindParameters, sig0);
    EOS_SET_METHOD(constructor_, "closeCursor", Statement, CloseCursor, sig0);
}

Handle<Value> Statement::New(const Arguments& args) {
    EOS_DEBUG_METHOD();

    HandleScope scope;

    if (args.Length() < 1)
        return ThrowError("Statement::New() requires at least one argument");

    if (!args.IsConstructCall()) {
        Handle<Value> argv[1] = { args[0] };
        return constructor_->GetFunction()->NewInstance(1, argv);
    }

    if (!Connection::Constructor()->HasInstance(args[0]))
        return ThrowTypeError("The first argument to Statement::New() must be a Connection");

    auto conn = ObjectWrap::Unwrap<Connection>(args[0]->ToObject());
    
    SQLHSTMT hStmt;

    auto ret = SQLAllocHandle(SQL_HANDLE_STMT, conn->GetHandle(), &hStmt);
    if (!SQL_SUCCEEDED(ret))
        return ThrowException(conn->GetLastError());

    auto hEvent = CreateEventW(nullptr, false, false, nullptr);
    if (!hEvent) {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return scope.Close(ThrowError("Unable to create wait handle"));
    }

    (new Statement(hStmt, conn, hEvent))->Wrap(args.Holder());

    return scope.Close(args.Holder());
}

Statement::Statement(SQLHSTMT hStmt, Connection* conn, HANDLE hEvent) 
    : EosHandle(SQL_HANDLE_STMT, hStmt, hEvent)
    , connection_(conn)
{
    EOS_DEBUG_METHOD();
}

Handle<Value> Statement::Cancel(const Arguments&) {
    EOS_DEBUG_METHOD();

    if(!SQL_SUCCEEDED(SQLCancelHandle(SQL_HANDLE_STMT, GetHandle())))
        return ThrowException(GetLastError());

    return Undefined();
}

Handle<Value> Statement::BindParameter(const Arguments& args) {
    EOS_DEBUG_METHOD();
    
    if (args.Length() < 4)
        return ThrowError("BindParameter expects 4, 5, or 6 arguments");
    
    if (!args[0]->IsInt32())
        return ThrowTypeError("The 1st argument should be an integer");

    if (!args[1]->IsInt32())
        return ThrowTypeError("The 2nd argument should be an integer");

    if (!args[2]->IsInt32())
        return ThrowTypeError("The 3rd argument should be an integer");

    // 0. parameter number (e.g. 2)
    // 1. parameter kind (e.g. SQL_PARAM_INPUT)
    // 2. SQL type (e.g. SQL_INTEGER)
    // 3. decimal digits (e.g. 0)
    // 4. value (e.g. 27)
    // 5. buffer to use (e.g. new Buffer(4))

    auto parameterNumber = args[0]->Int32Value();
    if (parameterNumber < 1 || parameterNumber > USHRT_MAX)
        return ThrowError("The parameter number is incorrect (valid values: 1 - 65535)");

    SQLSMALLINT inOutType = args[1]->Int32Value();
    SQLSMALLINT sqlType = args[2]->Int32Value();
    SQLSMALLINT decimalDigits = args[3]->Int32Value();

    Handle<Value> jsValue = Undefined();
    Handle<Object> bufferObject;

    if (args.Length() >= 5) 
        jsValue = args[4];

    if (args.Length() >= 6 && !args[5]->IsUndefined()) {
        if (!JSBuffer::HasInstance(args[5]) && !Buffer::HasInstance(args[5]))
            return ThrowTypeError("Argument 5 should be a Buffer or SlowBuffer");

        bufferObject = args[5].As<Object>();
    }

    auto jsParam = Parameter::Marshal(parameterNumber, inOutType, sqlType, decimalDigits, jsValue, bufferObject);
    if (jsParam.IsEmpty() || !jsParam->IsObject()) // Exception
        return jsParam;

    auto param = Parameter::Unwrap(jsParam.As<Object>());

    auto ret = SQLBindParameter(
        GetHandle(),
        parameterNumber,
        inOutType,
        param->CType(),
        param->SQLType(),
        param->Length(),
        decimalDigits,
        param->Buffer(),
        param->Length(),
        param->Indicator());

    if (!SQL_SUCCEEDED(ret))
        return ThrowException(GetLastError());
  
    Statement::AddBoundParameter(param);

    return jsParam;
}

void Statement::AddBoundParameter(Parameter* param) {
    EOS_DEBUG_METHOD();

    if (bindings_.IsEmpty())
        bindings_ = Persist(Array::New());

    bindings_->Set(param->ParameterNumber(), param->handle_);
}

Handle<Value> Statement::SetParameterName(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (args.Length() != 2)
        return ThrowError("Statement::SetParameterName requires 2 arguments");

    if (!args[0]->IsInt32())
        return ThrowError("The parameter number must be an integer");

    auto parameterNumber = args[0]->Int32Value();
    if (parameterNumber < 1 || parameterNumber >= USHRT_MAX)
        return ThrowError("The parameter number must be between 1 and 65535 inclusive");

    WStringValue name(args[1]);

    SQLHDESC hIpd;

    auto ret = SQLGetStmtAttr(
        GetHandle(), 
        SQL_ATTR_IMP_PARAM_DESC, 
        &hIpd, 0, 0);

    if (!SQL_SUCCEEDED(ret))
        return ThrowException(GetLastError());

    ret = SQLSetDescFieldW(
        hIpd, 
        parameterNumber, 
        SQL_DESC_NAME, 
        *name, 
        name.length() * sizeof(**name));

    if (!SQL_SUCCEEDED(ret))
        return ThrowException(Eos::GetLastError(SQL_HANDLE_DESC, hIpd));

    return Undefined();
}

Parameter* Statement::GetBoundParameter(SQLUSMALLINT parameterNumber) {
    EOS_DEBUG_METHOD_FMT(L"%i", parameterNumber);

    if (bindings_.IsEmpty())
        return nullptr;

    auto handle = bindings_->Get(parameterNumber);
    if (handle->IsObject())
        return Parameter::Unwrap(handle.As<Object>());

    return nullptr;
}

Handle<Value> Statement::UnbindParameters(const Arguments&) {
    EOS_DEBUG_METHOD();

    if(!SQL_SUCCEEDED(SQLFreeStmt(GetHandle(), SQL_RESET_PARAMS)))
        return ThrowException(GetLastError());

    bindings_.Clear();

    return Undefined();
}

Handle<Value> Statement::CloseCursor(const Arguments& args) {
    EOS_DEBUG_METHOD();

    SQLRETURN ret;
    if (args.Length() > 0 && args[0]->IsTrue())
        ret = SQLCloseCursor(GetHandle()); // Can fail if no open cursor
    else
        ret = SQLFreeStmt(GetHandle(), SQL_CLOSE); // No error if no cursor

    if(!SQL_SUCCEEDED(ret))
        return ThrowException(GetLastError());

    return Undefined();
}

Statement::~Statement() {
    EOS_DEBUG_METHOD();
}

namespace { ClassInitializer<Statement> c; }
