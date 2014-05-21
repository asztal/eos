#include "handle.hpp"

using namespace Eos;

EosHandle::EosHandle(SQLSMALLINT handleType, const SQLHANDLE handle, HANDLE hEvent)
    : handleType_(handleType)
    , sqlHandle_(handle)
    , hEvent_(hEvent)
    , hWait_(nullptr)
{
    EOS_DEBUG_METHOD_FMT(L"handleType = %i", handleType);
}

EosHandle::~EosHandle() {
    EOS_DEBUG_METHOD_FMT(L"handleType = %i, handle = 0x%p", handleType_, handle());
    
    assert(operation_.IsEmpty());

    FreeHandle();

    assert(!hWait_ && "The handle should not be destructed until Notify() "
        "is called (perhaps there is some sort of Ref()/Unref() mismatch)");

    if (hEvent_) {
        CloseHandle(hEvent_);
        hWait_ = nullptr;
        hEvent_ = nullptr;
    }
}

void EosHandle::Init ( const char* className
                     , Persistent<FunctionTemplate>& constructor
                     , NanFunctionCallback New) 
{
    auto ft = NanNew<FunctionTemplate>(New);
    NanAssignPersistent(constructor, ft);

    ft->SetClassName(NanSymbol(className));
    ft->InstanceTemplate()->SetInternalFieldCount(1);

    auto sig0 = NanNew<Signature>(ft);
    EOS_SET_METHOD(ft, "free", EosHandle, Free, sig0);
}

void EosHandle::Notify() {
    EOS_DEBUG_METHOD_FMT(L"handleType = %i", handleType_);
    
    assert(!operation_.IsEmpty());

    NanScope();
    Handle<Object> op = NanNew(operation_);
    NanDisposePersistent(operation_);
    hWait_ = nullptr;

    ObjectWrap::Unwrap<IOperation>(op)->OnCompleted();
}

NAN_METHOD(EosHandle::Free) {
    EOS_DEBUG_METHOD_FMT(L"handleType = %i", handleType_);

    auto ret = FreeHandle();
    if (!SQL_SUCCEEDED(ret))
        return NanThrowError(GetLastError());

    NanReturnUndefined();
}

SQLRETURN EosHandle::FreeHandle() {
    EOS_DEBUG_METHOD_FMT(L"handleType = %i", handleType_);

    if (!IsValid())
        return SQL_SUCCESS_WITH_INFO;

    if (hWait_ || !operation_.IsEmpty())
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

    NanDisposePersistent(operation_);

    return SQL_SUCCESS;
}
