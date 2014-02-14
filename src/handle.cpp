#include "handle.hpp"

using namespace Eos;

EosHandle::EosHandle(SQLSMALLINT handleType, const SQLHANDLE handle, HANDLE hEvent)
    : handleType_(handleType)
    , sqlHandle_(handle)
    , hEvent_(hEvent)
    , hWait_(nullptr)
{
    EOS_DEBUG_METHOD();
}

EosHandle::~EosHandle() {
    EOS_DEBUG_METHOD();

    FreeHandle();

    assert(!hWait_ && "The handle should not be destructed until Notify() "
        "is called (perhaps there is some sort of Ref)/Unref() mismatch)");

    if (hEvent_) {
        CloseHandle(hEvent_);
        hWait_ = nullptr;
        hEvent_ = nullptr;
    }
}

void EosHandle::Init ( const char* className
                     , Handle<FunctionTemplate>& ft
                     , InvocationCallback New) 
{
    ft = Persistent<FunctionTemplate>::New(FunctionTemplate::New(New));
    ft->SetClassName(String::NewSymbol(className));
    ft->InstanceTemplate()->SetInternalFieldCount(1);

    auto sig0 = Signature::New(ft);
    EOS_SET_METHOD(ft, "free", EosHandle, Free, sig0);
}

void EosHandle::Notify() {
    EOS_DEBUG_METHOD();
    
    HandleScope scope;
    Handle<Object> op = operation_;
    operation_.Dispose();
    hWait_ = nullptr;

    ObjectWrap::Unwrap<IOperation>(op)->OnCompleted();
}

Handle<Value> EosHandle::Free(const Arguments& args) {
    EOS_DEBUG_METHOD();

    auto ret = FreeHandle();
    if (!SQL_SUCCEEDED(ret))
        return ThrowException(GetLastError());

    return Undefined();
}

SQLRETURN EosHandle::FreeHandle() {
    if (!IsValid())
        return SQL_SUCCESS_WITH_INFO;

    if (hWait_)
        EOS_DEBUG(L"Attempted to call FreeHandle() while an asynchronous operation is executing\n");

    auto ret = SQLFreeHandle(handleType_, sqlHandle_);
    if (!SQL_SUCCEEDED(ret))
        return ret;

    sqlHandle_ = SQL_NULL_HANDLE;

    if (hEvent_) {
        if(!CloseHandle(hEvent_))
            EOS_DEBUG(L"Failed to close hEvent_\n");
        hEvent_ = nullptr;
    }

    return SQL_SUCCESS;
}