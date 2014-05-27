#pragma once 

#include "eos.hpp"

#include <uv.h>

#if defined(DEBUG)
#include <vector>
#include <algorithm> 
#endif

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

    protected:
        static std::vector<IOperation*> activeOperations_;

    public:
        static NAN_METHOD(DebugActiveOperations) {
            NanScope();

            EOS_DEBUG(L"Active operations:\n");
            EOS_DEBUG(L"------------------\n");

            for(auto it = activeOperations_.begin();
                it != activeOperations_.end();
                ++it)
            {
                EOS_DEBUG(L"\n\n%hs operation started at:\n", (*it)->Name());
                PrintStackTrace((*it)->GetStackTrace());
            }

            NanReturnUndefined();
        }

        static NAN_METHOD(GetActiveOperations) {
            NanScope();

            auto ops = NanNew<Array>();

            for(auto it = activeOperations_.begin();
                it != activeOperations_.end();
                ++it)
            {
                ops->Set(ops->Length(), NanObjectWrapHandle((*it)));
            }

            NanReturnValue(ops);
        }
#endif

    protected:
        virtual const char* Name() const = 0;

        virtual SQLRETURN CallOverride() = 0;

        virtual void OnBegin() {}
        
        virtual void CallbackOverride(SQLRETURN ret) {
            // Default implementation.
            EOS_DEBUG_METHOD();
            
            if (!SQL_SUCCEEDED(ret))
                return CallbackErrorOverride(ret);
            
            NanMakeCallback(NanGetCurrentContext()->Global(), GetCallback(), 0, nullptr);
        }
        
        virtual void CallbackErrorOverride(SQLRETURN ret) = 0;

        Handle<Function> GetCallback() const {
            return NanNew(callback_);
        }

        template <size_t argc>
        void Callback(Handle<Value> (&argv)[argc]) {
            NanMakeCallback(NanGetCurrentContext()->Global(), GetCallback(), argc, argv);
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
            Constructor()->InstanceTemplate()->Set(NanSymbol("name"), NanNew<String>(TOp::Name()));
            Constructor()->InstanceTemplate()->SetInternalFieldCount(1);

#if defined(DEBUG)
#define EOS_COMMA ,
            EOS_SET_GETTER(Constructor(), "stackTrace", Operation<TOwner EOS_COMMA TOp>, StackTraceGetter);
            EOS_SET_GETTER(Constructor(), "refs", Operation<TOwner EOS_COMMA TOp>, RefsGetter);
#undef EOS_COMMA
#endif
        }

#if defined(DEBUG)
        NAN_GETTER(StackTraceGetter) const {
            auto frames = NanNew<Array>();
            auto stackTrace = GetStackTrace();
            auto frameCount = stackTrace->GetFrameCount();
            
            auto kFile = NanSymbol("sourceFile");
            auto kFunction = NanSymbol("functionName");
            auto kLine = NanSymbol("lineNumber");

            for (int i = 0; i < frameCount; i++) {
                auto frame = stackTrace->GetFrame(i);
                auto jsFrame = NanNew<Object>();
                
                auto file = frame->GetScriptNameOrSourceURL();
                if (file.IsEmpty())
                    file = NanNew<String>("<unknown>");
                
                auto fun = frame->GetFunctionName();
                if (fun.IsEmpty())
                    fun = NanNew<String>("<unknown>");
                
                jsFrame->Set(kFile, file);
                jsFrame->Set(kFunction, fun);
                jsFrame->Set(kLine, NanNew<Integer>(frame->GetLineNumber()));
                
                frames->Set(i, jsFrame);
            }

            NanReturnValue(frames);
        }
        
        NAN_GETTER(RefsGetter) const {
            NanReturnValue(NanNew<Integer>(refs_));
        }
#endif

        Operation() : completed_(false) { 
            EOS_DEBUG_METHOD();
            
#if defined(DEBUG)
            activeOperations_.push_back(this);
#endif
        }
        
        ~Operation() { 
            EOS_DEBUG_METHOD();

            ownerPtr_ = nullptr;
            assert(completed_);
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
            auto retCA = SQLCompleteAsync(TOwner::HandleType, Owner()->GetHandle(), &ret);

            assert(SQL_SUCCEEDED(retCA) && "OnCompleted() called before operation actually completed");

            Complete(ret);
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
        const char* Name() const {
            return TOp::Name();
        }

        void Complete(SQLRETURN ret) {
#if defined(DEBUG)
            auto n0 = activeOperations_.size();
            auto end = remove(activeOperations_.begin(), activeOperations_.end(), this);
            activeOperations_.erase(end, activeOperations_.end());
            auto n1 = activeOperations_.size();
            assert(n1 < n0);
            assert(n1 == n0 - 1);
#endif

            TryCatch tc;
            CallbackOverride(ret);
            if (tc.HasCaught())
                FatalException(tc);

            Owner()->Unref();

            assert(refs_ == 1);
            this->Unref();
        }

        void CallbackErrorOverride(SQLRETURN ret) {
            Handle<Value> argv[] = { Owner()->GetLastError() };
            NanMakeCallback(NanGetCurrentContext()->Global(), GetCallback(), 1, argv);
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

            assert(!op->completed_);
            op->completed_ = true;

            op->Complete(op->result_);
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
