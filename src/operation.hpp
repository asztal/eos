#pragma once 

#include "eos.hpp"

#include <uv.h>

namespace Eos {
    struct IOperation : ObjectWrap {
        IOperation() {
            EOS_DEBUG_METHOD();
        
#if defined(DEBUG)
            NanAssignPersistent(stackTrace_, 
                IF_NODE_12
                    ( StackTrace::CurrentStackTrace(nan_isolate, 10)
                    , StackTrace::CurrentStackTrace(10))
                );
#endif
        }

        virtual ~IOperation() {
            EOS_DEBUG_METHOD();
        }

        SQLRETURN Call() {
            return this->CallOverride();
        }

        void Callback(SQLRETURN ret) {
            TryCatch tc;
            this->CallbackOverride(ret);
            if (tc.HasCaught())
                FatalException(tc);
        }

        virtual void OnCompleted() = 0;

#if defined(DEBUG)
        Handle<StackTrace> GetStackTrace() const {
            return NanNew(stackTrace_);
        }
#endif

    protected:
        virtual SQLRETURN CallOverride() = 0;

        virtual void OnBegin() {}
        
        virtual void CallbackOverride(SQLRETURN ret) {
            // Default implementation.
            EOS_DEBUG_METHOD();
            
            if (!SQL_SUCCEEDED(ret))
                return CallbackErrorOverride(ret);
            
            GetCallback()->Call(NanGetCurrentContext()->Global(), 0, nullptr);
        }
        
        virtual void CallbackErrorOverride(SQLRETURN ret) = 0;

        Handle<Function> GetCallback() const {
            return NanNew(callback_);
        }

        template <size_t argc>
        void Callback(Handle<Value> (&argv)[argc]) {
            NanNew(callback_)->Call(NanGetCurrentContext()->Global(), argc, argv);
        }

    protected:
        Persistent<Function> callback_;

#if defined(DEBUG)
        Persistent<StackTrace> stackTrace_;
#endif
    };

    template <class TOwner, class TOp>
    struct Operation : IOperation {
        static void Init(Handle<Object> exports) {
            EOS_DEBUG_METHOD();

            NanAssignPersistent(constructor_, NanNew<FunctionTemplate>(New));
            Constructor()->SetClassName(NanSymbol(TOp::Name()));
            Constructor()->InstanceTemplate()->SetInternalFieldCount(1);
        }

        Operation() : completed_(false) { 
            EOS_DEBUG_METHOD();
        }
        
        ~Operation() { 
            ownerPtr_ = nullptr;
            assert(completed_);
            EOS_DEBUG_METHOD();
        }

        template<size_t argc>
        static Local<Object> Construct(Handle<Value> (&argv)[argc]) {
            return Constructor()->GetFunction()->NewInstance(argc, argv); 
        }

        static NAN_METHOD(New) {
            EOS_DEBUG_METHOD();
            
            NanScope();

            if (args.Length() < 2)
                return NanThrowError("Too few arguments");

            if (!args.IsConstructCall()) {
                EOS_DEBUG(L"Warning: %ls called, but args.IsConstructCall() is false\n", __FUNCTIONW__);
#if defined(DEBUG)
                PrintStackTrace();
#endif
                // XXX There must be a better way...
                auto argc = args.Length();
                auto argv = new Handle<Value>[argc];
                for (int i = 0; i < argc; i++)
                    argv[i] = args[i];
                auto ret = NanNew(constructor_)->GetFunction()->NewInstance(args.Length(), argv);
                delete[] argv;
                NanReturnValue(ret);
            }

            if (!TOwner::Constructor()->HasInstance(args[0]))
                return NanThrowTypeError("Bad argument");

            if (!args[args.Length() - 1]->IsFunction())
                return NanThrowTypeError("Last argument should be a callback function");

            auto owner = ObjectWrap::Unwrap<TOwner>(args[0]->ToObject());
            
            auto error = TOp::New(owner, args);
            if (!error->IsUndefined())
                return NanThrowError(error);

            auto op = ObjectWrap::Unwrap<TOp>(args.Holder());
            op->ownerPtr_ = owner;
            NanAssignPersistent(op->owner_, NanObjectWrapHandle(owner));
            NanAssignPersistent(op->callback_, args[args.Length() - 1].As<Function>());

            NanReturnValue(args.Holder());
        };

        static Handle<FunctionTemplate> Constructor() {
            return NanNew(constructor_);
        }

        TOwner* Owner() { return ownerPtr_; }

        // Returns true if the operation completed synchronously.
        bool BeginAsync(bool isComplete = false) {
            EOS_DEBUG_METHOD();
            
            OnBegin();

            auto ret = Call();

            // As explained by the MSDN documentation, enabling asynchronous
            // notifications on a connection handle always succeeds if it is 
            // not connected, because it doesn't know which driver it will be
            // using. If the driver does not support asynchronous operations, 
            // it will return SQL_ERROR with SQLSTATE S1118.
            if (ret == SQL_ERROR) {
                wchar_t state[6];

                // The documentation doesn't explicitly say that you can 
                // pass nullptr instead of pointers to these.
                SQLSMALLINT messageLength;
                SQLINTEGER nativeError;

                auto diagRet = SQLGetDiagRecW(
                    Owner()->GetHandleType(), 
                    Owner()->GetHandle(),
                    1,
                    state,
                    &nativeError,
                    nullptr, 0, &messageLength);

                if (diagRet == SQL_SUCCESS && wcscmp(state, L"S1118") == 0) {
                    Owner()->DisableAsynchronousNotifications();
                    
                    // Now try again. If it fails again for the same reason,
                    // pass the error back to JS.
                    ret = Call();
                }
            }

            EOS_DEBUG(L"Immediate Result: %hi\n", ret);
            
            if (ret == SQL_STILL_EXECUTING) {
                assert(!isComplete);
                return false; // Asynchronous
            }
        
            Callback(ret);
            return true; // Synchronous
        }

        // Completed using asynchronous notifications
        void OnCompleted() {
            EOS_DEBUG_METHOD_FMT(L"owner: 0x%p, operation: 0x%p", Owner(), this);
            assert(!completed_);
            completed_ = true;

            SQLRETURN ret;
            SQLCompleteAsync(TOwner::HandleType, Owner()->GetHandle(), &ret);
            
            TryCatch tc;
            CallbackOverride(ret);
            if (tc.HasCaught())
                FatalException(tc);

            Owner()->Unref();
            this->Unref();
        }

        void RunOnThreadPool() {
            EOS_DEBUG_METHOD();

            OnBegin();

            new(&req_) uv_work_t;
            req_.data = this;

            uv_queue_work(
                uv_default_loop(),
                &req_,
                &UVWorkCallback,
                &UVCompletedCallback);
        }

    protected:
        void CallbackErrorOverride(SQLRETURN ret) {
            Handle<Value> argv[] = { Owner()->GetLastError() };
            GetCallback()->Call(NanGetCurrentContext()->Global(), 1, argv);
        }

        static void UVWorkCallback(uv_work_t* req) {
            auto op = static_cast<Operation<TOwner, TOp>*>(req->data);
            assert(op && &op->req_ == req);

            op->result_ = op->Call();
        }

        static void UVCompletedCallback(uv_work_t* req, int status) {
            auto op = static_cast<Operation<TOwner, TOp>*>(req->data);
            assert(op && &op->req_ == req);

            NanScope();

            op->completed_ = true;
            
            TryCatch tc;
            op->CallbackOverride(op->result_);
            if (tc.HasCaught())
                FatalException(tc);

            op->Owner()->Unref();
            op->Unref();
        }

    private:
        void OnBegin() {
            EOS_DEBUG_METHOD();
            Owner()->Ref();
            this->Ref();
        }

        // For thread pool tasks
        uv_work_t req_;
        SQLRETURN result_;

        bool completed_;
        TOwner* ownerPtr_;
        Persistent<Object> owner_;
        static Persistent<FunctionTemplate> constructor_;
    };
}
