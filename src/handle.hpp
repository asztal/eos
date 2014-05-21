#pragma once 

#include "eos.hpp"
#include "operation.hpp"

namespace Eos {
    struct EosHandle: ObjectWrap, INotify {
        EosHandle(SQLSMALLINT handleType, const SQLHANDLE handle, HANDLE hEvent);
        ~EosHandle();
        
        NAN_METHOD(Free);
        SQLHANDLE GetHandle() const { return sqlHandle_; }
        Local<Value> GetLastError() { return Eos::GetLastError(handleType_, sqlHandle_); }
        IOperation* Operation() { return ObjectWrap::Unwrap<IOperation>(NanNew(operation_)); }

        // INotify
        void INotify::Ref() { EOS_DEBUG_METHOD_FMT(L"handleType = %i", handleType_); ObjectWrap::Ref(); }
        void INotify::Unref() { EOS_DEBUG_METHOD_FMT(L"handleType = %i", handleType_); ObjectWrap::Unref(); }
        HANDLE /*INotify::*/GetEventHandle() const { return hEvent_; } // Including interface name gives strange IntelliSense errors
        HANDLE /*INotify::*/GetWaitHandle() const { return hWait_; } 
        void INotify::Notify();

    protected:
        static void Init(
            const char* className, 
            Persistent<FunctionTemplate>& ft, 
            NanFunctionCallback New);

        template<typename TOp, size_t argc>
        _NAN_METHOD_RETURN_TYPE Begin(Handle<Value> (&argv)[argc]) {
            if (!IsValid())
                return NanThrowError("This handle has been freed.");

            if (hWait_)
                return NanThrowError("This handle is already busy.");

            IOperation* currentOp = nullptr;
            if (!operation_.IsEmpty()) {
                currentOp = ObjectWrap::Unwrap<IOperation>(NanNew(operation_));
                return NanThrowError("An operation is already in progress on this handle.");
            }

            auto op = TOp::Construct(argv).As<Object>();
            if (op.IsEmpty())
                NanReturnUndefined(); // Probably the constructor threw

            bool completedSynchronously = 
                ObjectWrap::Unwrap<TOp>(op)->Begin();

            // Only register a wait if it did not complete synchronously
            // It should be safe to register the wait even if the event is already signalled.
            if (!completedSynchronously) {
                EOS_DEBUG(L"%hs Executing asynchronously\n", TOp::Name());
                if (hWait_ = Eos::Wait(this))
                    NanAssignPersistent(operation_, op);
                else 
                    return NanThrowError(OdbcError("Unable to begin asynchronous operation"));
            } else {
                EOS_DEBUG(L"%hs Completed synchronously\n", TOp::Name());
            }
            
            NanReturnUndefined();
        }

        bool IsValid() const { return sqlHandle_ != SQL_NULL_HANDLE; }
        SQLRETURN FreeHandle();

    private:
        EosHandle::EosHandle(const EosHandle& other); // = delete;
        
        HANDLE hEvent_, hWait_;
        Persistent<Object> operation_;
        SQLHANDLE sqlHandle_;
        SQLSMALLINT handleType_; 
    };
}