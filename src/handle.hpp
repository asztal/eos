#pragma once 

#include "eos.hpp"

namespace Eos {
    struct EosHandle: ObjectWrap, INotify {
        EosHandle(SQLSMALLINT handleType, const SQLHANDLE handle, HANDLE hEvent);
        ~EosHandle();
        
        Handle<Value> Free(const Arguments& args);
        SQLHANDLE GetHandle() const { return sqlHandle_; }
        Local<Value> GetLastError() { return Eos::GetLastError(handleType_, sqlHandle_); }
        IOperation* Operation() { return ObjectWrap::Unwrap<IOperation>(operation_); }

        // INotify
        void INotify::Ref() { EOS_DEBUG_METHOD(); ObjectWrap::Ref(); }
        void INotify::Unref() { EOS_DEBUG_METHOD(); ObjectWrap::Unref(); }
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

            operation_ = Persistent<Object>::New(TOp::Construct(argv).As<Object>());

            hWait_ = Eos::Wait(this);
            if (!hWait_) {
                operation_.Dispose();
                return ThrowException(OdbcError("Unable to begin asynchronous operation"));
            }

            ObjectWrap::Unwrap<TOp>(operation_)->Begin();
            
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