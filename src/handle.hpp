#pragma once 

#include "eos.hpp"
#include "operation.hpp"

namespace Eos {
    struct EosHandle: ObjectWrap, INotify {
        EosHandle(SQLSMALLINT handleType, const SQLHANDLE handle, HANDLE hEvent);
        ~EosHandle();
        
        Handle<Value> Free(const Arguments& args);
        SQLHANDLE GetHandle() const { return sqlHandle_; }
        Local<Value> GetLastError() { return Eos::GetLastError(handleType_, sqlHandle_); }
        IOperation* Operation() { return ObjectWrap::Unwrap<IOperation>(operation_); }

        // INotify
        void INotify::Ref() { EOS_DEBUG_METHOD_FMT(L"handleType = %i", handleType_); ObjectWrap::Ref(); }
        void INotify::Unref() { EOS_DEBUG_METHOD_FMT(L"handleType = %i", handleType_); ObjectWrap::Unref(); }
        HANDLE /*INotify::*/GetEventHandle() const { return hEvent_; } // Including interface name gives strange IntelliSense errors
        HANDLE /*INotify::*/GetWaitHandle() const { return hWait_; } 
        void INotify::Notify();

    protected:
        static void Init(
            const char* className, 
            Handle<FunctionTemplate>& ft, 
            InvocationCallback New);

        template<typename TOp, size_t argc>
        Handle<Value> Begin(Handle<Value> (&argv)[argc]) {
            if (!IsValid())
                return ThrowError("This handle has been freed.");

            if (hWait_)
                return ThrowError("This handle is already busy.");

            IOperation* currentOp = nullptr;
            if (!operation_.IsEmpty()) {
                currentOp = ObjectWrap::Unwrap<IOperation>(operation_);
                return ThrowError("An operation is already in progress on this handle.");
            }

            auto op = TOp::Construct(argv).As<Object>();
            if (op.IsEmpty())
                return op; // Probably the constructor threw

            bool completedSynchronously = 
                ObjectWrap::Unwrap<TOp>(op)->Begin();

            // Only register a wait if it did not complete synchronously
            // It should be safe to register the wait even if the event is already signalled.
            if (!completedSynchronously) {
                EOS_DEBUG(L"%hs Executing asynchronously\n", TOp::Name());
                if (hWait_ = Eos::Wait(this))
                    operation_ = Persistent<Object>::New(op);
                else 
                    return ThrowException(OdbcError("Unable to begin asynchronous operation"));
            } else {
                EOS_DEBUG(L"%hs Completed synchronously\n", TOp::Name());
            }
            
            return Undefined();
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