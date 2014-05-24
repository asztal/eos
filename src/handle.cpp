#include "handle.hpp"

using namespace Eos;

EosHandle::EosHandle(SQLSMALLINT handleType, const SQLHANDLE handle EOS_ASYNC_ONLY_ARG(HANDLE hEvent))
    : handleType_(handleType)
    , sqlHandle_(handle)
#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
    , hEvent_(hEvent)
    , hWait_(nullptr)
#endif
{
#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
    EOS_DEBUG_METHOD_FMT(L"handleType = %i, hEvent = 0x%p", handleType, hEvent);
#else
    EOS_DEBUG_METHOD_FMT(L"handleType = %i", handleType);
#endif
}

EosHandle::~EosHandle() {
    EOS_DEBUG_METHOD_FMT(L"handleType = %i, handle = 0x%p", handleType_, sqlHandle_);
    
    assert(operation_.IsEmpty() && "The handle should not be destructed while an operation is in progress");

    FreeHandle();
    
#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
    assert(!hWait_ && "The handle should not be destructed until Notify() "
        "is called (perhaps there is some sort of Ref()/Unref() mismatch)");

    if (hEvent_) {
        CloseHandle(hEvent_);
        hWait_ = nullptr;
        hEvent_ = nullptr;
    }
#endif
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

#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
void EosHandle::Notify() {
    EOS_DEBUG_METHOD_FMT(L"handleType = %i", handleType_);
    
    assert(!operation_.IsEmpty());

    NanScope();
    Handle<Object> op = NanNew(operation_);
    NanDisposePersistent(operation_);
    hWait_ = nullptr;

    ObjectWrap::Unwrap<IOperation>(op)->OnCompleted();
}

void EosHandle::DisableAsynchronousNotifications() {
    EOS_DEBUG_METHOD();

    assert(!hWait_ && "Attempted to disable asynchronous notifications "
        "while an asynchronous operation is in progress");

    if (hEvent_) {
        if(!CloseHandle(hEvent_))
            EOS_DEBUG(L"Failed to close hEvent_\n");
        hEvent_ = nullptr;
    }
}
#endif

// Notify that a task on the thread pool completed. Not quite the
// same as completing an asynchronous task, since the callback has
// already been called.
void EosHandle::NotifyThreadPool() {
    EOS_DEBUG_METHOD_FMT(L"handleType = %i", handleType_);
    
    assert(!operation_.IsEmpty());

    NanScope();

    NanDisposePersistent(operation_);
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

    bool executing = !operation_.IsEmpty();
#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
    if (hWait_)
        executing = true;
#endif

    if (executing)
        EOS_DEBUG(L"Attempted to call FreeHandle() while an asynchronous operation is executing\n");

    auto ret = SQLFreeHandle(handleType_, sqlHandle_);
    if (!SQL_SUCCEEDED(ret))
        return ret;

    sqlHandle_ = SQL_NULL_HANDLE;

#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
    if (hEvent_) {
        if(!CloseHandle(hEvent_))
            EOS_DEBUG(L"Failed to close hEvent_\n");
        hEvent_ = nullptr;
    }
#endif

    NanDisposePersistent(operation_);

    return SQL_SUCCESS;
}
