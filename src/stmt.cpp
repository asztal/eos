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
    EOS_SET_METHOD(constructor_, "bindParameter", Statement, BindParameter, sig0);
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
        return ThrowError("BindParameter expects 4 or 5 arguments");

    auto parameterNumber = args[0]->Int32Value();
    if (parameterNumber < 1 || parameterNumber > USHRT_MAX)
        return ThrowError("The parameter number is incorrect (valid values: 1 - 65535)");

    SQLSMALLINT inOutType = args[1]->Int32Value();
    SQLSMALLINT sqlType = args[2]->Int32Value();
    SQLSMALLINT decimalDigits = args[3]->Int32Value();

    Handle<Value> jsValue = Undefined();
    if (args.Length() >= 5) 
        jsValue = args[4];

    auto param = Parameter::Marshal(parameterNumber, inOutType, decimalDigits, jsValue, sqlType);
    if (!param)
        return ThrowError("The specified value is invalid.");

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
        param->LenBuffer());

    if (!SQL_SUCCEEDED(ret))
        return ThrowException(GetLastError());
  
    Statement::AddBoundParameter(param);

    return Undefined();
}

void Statement::AddBoundParameter(Parameter* param) {
    EOS_DEBUG_METHOD();

    if (bindings_.IsEmpty())
        bindings_ = Persist(Array::New());

    bindings_->Set(param->ParameterNumber(), param->handle_);
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

void Statement::ClearBoundParameters() {
    EOS_DEBUG_METHOD();

    bindings_.Clear();
}

Handle<Value> Statement::CloseCursor(const Arguments& args) {
    EOS_DEBUG_METHOD();

    SQLRETURN ret;
    if (args.Length() > 0 && args[0]->IsTrue())
        ret = SQLCloseCursor(GetHandle());
    else
        ret = SQLFreeStmt(GetHandle(), SQL_CLOSE);

    if(!SQL_SUCCEEDED(ret))
        return ThrowException(GetLastError());

    return Undefined();
}

Statement::~Statement() {
    EOS_DEBUG_METHOD();
}

namespace { ClassInitializer<Statement> c; }
