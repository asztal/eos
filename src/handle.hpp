#pragma once 

#include "eos.hpp"
#include "operation.hpp"

#include <uv.h>

#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
#define EOS_ASYNC_ONLY_ARG(x) , x
#else
#define EOS_ASYNC_ONLY_ARG(x)
#endif

namespace Eos {
    struct EosHandle: ObjectWrap EOS_ASYNC_ONLY_ARG(INotify) {

        EosHandle(SQLSMALLINT handleType, const SQLHANDLE handle EOS_ASYNC_ONLY_ARG(HANDLE hEvent));
        ~EosHandle();
        
        NAN_METHOD(Free);
        SQLHANDLE GetHandle() const { return sqlHandle_; }
        SQLSMALLINT GetHandleType() const { return handleType_; }
        Handle<Value> GetLastError() { return Eos::GetLastError(handleType_, sqlHandle_); }
        IOperation* Operation() { return ObjectWrap::Unwrap<IOperation>(NanNew(operation_)); }
        
#if defined(DEBUG)
        static std::vector<EosHandle*> active_;
        static NAN_METHOD(GetActiveHandles);
#endif

        void Ref() { 
            EOS_DEBUG_METHOD_FMT(L"this = 0x%p, handleType = %i, refs = %i++", this, handleType_, refs_);
#if defined(DEBUG)
            if (refs_ == 0) 
                active_.push_back(this);
#endif
            ObjectWrap::Ref(); 
        }

        void Unref() { 
            EOS_DEBUG_METHOD_FMT(L"this = 0x%p, handleType = %i, refs = %i--", this, handleType_, refs_);
#if defined(DEBUG)
            if (refs_ == 1) {
                auto it = remove(active_.begin(), active_.end(), this);
                active_.erase(it, active_.end());
            }
#endif
            ObjectWrap::Unref(); 

        }

#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
        // INotify
        HANDLE /*INotify::*/GetEventHandle() const { return hEvent_; } // Including interface name gives strange IntelliSense errors
        HANDLE /*INotify::*/GetWaitHandle() const { return hWait_; } 
        void INotify::Notify();        

        virtual void DisableAsynchronousNotifications();
#endif

        void NotifyThreadPool();

    protected:
        static void Init(
            const char* className, 
            Persistent<FunctionTemplate>& ft, 
            NanFunctionCallback New);

        template<typename TOp, size_t argc>
        _NAN_METHOD_RETURN_TYPE Begin(Handle<Value> (&argv)[argc]) {
            if (!IsValid())
                return NanThrowError("This handle has been freed.");
            
#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
            if (hWait_)
                return NanThrowError("This handle is already busy.");
#endif

            IOperation* currentOp = nullptr;
            if (!operation_.IsEmpty()) {
                currentOp = ObjectWrap::Unwrap<IOperation>(NanNew(operation_));
                return NanThrowError("An operation is already in progress on this handle.");
            }

            auto op = TOp::Construct(argv).As<Object>();
            if (op.IsEmpty())
                NanReturnUndefined(); // Probably the constructor threw

#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
            if (hEvent_) {
                RunAsync<TOp>(op);
                NanReturnUndefined();
            }
#endif
            
            ObjectWrap::Unwrap<TOp>(op)->RunOnThreadPool();

            NanReturnUndefined();
        }

#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
        template <typename TOp>
        void RunAsync(Handle<Object> op) {
            bool completedSynchronously = 
                ObjectWrap::Unwrap<TOp>(op)->BeginAsync();

            // Only register a wait if it did not complete synchronously
            // It should be safe to register the wait even if the event is already signalled.
            if (completedSynchronously) {
                EOS_DEBUG(L"%hs Completed synchronously\n", TOp::Name());
                return;
            }

            EOS_DEBUG(L"%hs Executing asynchronously\n", TOp::Name());
            if (hWait_ = Eos::Wait(this))
                NanAssignPersistent(operation_, op);
            else { 
                NanThrowError(OdbcError("Unable to begin asynchronous operation"));
                return;
            }

        }
#endif

        bool IsValid() const { return sqlHandle_ != SQL_NULL_HANDLE; }
        SQLRETURN FreeHandle();

    private:
        EosHandle::EosHandle(const EosHandle& other); // = delete;
        
#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
        HANDLE hEvent_, hWait_;
#endif

        Persistent<Object> operation_;
        SQLHANDLE sqlHandle_;
        SQLSMALLINT handleType_; 
    };
}