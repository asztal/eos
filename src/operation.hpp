#pragma once 

#include "eos.hpp"

#include <uv.h>

#if defined(DEBUG)
#define DEBUG_ONLY(x) x
#include <vector>
#include <algorithm>
#else
#define DEBUG_ONLY(x)
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
        
#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
        virtual void OnCompletedAsync() = 0;
#endif

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
                EOS_DEBUG(L"\n\n%hs operation started at:\n", (*it)->GetName());
                PrintStackTrace((*it)->GetStackTrace());
            }

            NanReturnUndefined();
        }

        static NAN_METHOD(GetActiveOperations) {
            NanScope();
            

#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
            DebugWaitCounters();
#endif

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

        virtual const char* GetName() const = 0;

    protected:

        virtual SQLRETURN CallOverride() = 0;
        virtual void CallbackOverride(SQLRETURN ret) = 0;
        virtual void CallbackErrorOverride(SQLRETURN ret) = 0;

        Handle<Function> GetCallback() const {
            return NanNew(callback_);
        }

    protected:
        Persistent<Function> callback_;

        DEBUG_ONLY(Persistent<StackTrace> stackTrace_);
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
            EOS_SET_GETTER(Constructor(), "begun", Operation<TOwner EOS_COMMA TOp>, BegunGetter);
            EOS_SET_GETTER(Constructor(), "completed", Operation<TOwner EOS_COMMA TOp>, CompletedGetter);
            EOS_SET_GETTER(Constructor(), "sync", Operation<TOwner EOS_COMMA TOp>, SyncGetter);
            EOS_SET_GETTER(Constructor(), "result", Operation<TOwner EOS_COMMA TOp>, ResultGetter);
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

            EosMethodReturnValue(frames);
        }
        
        NAN_GETTER(RefsGetter) const {
            EosMethodReturnValue(NanNew<Integer>(refs_));
        }
        
        NAN_GETTER(ResultGetter) const {
            EosMethodReturnValue(NanNew<Integer>(result_));
        }
        
        NAN_GETTER(BegunGetter) const {
            EosMethodReturnValue(NanNew<Boolean>(begun_));
        }
        
        NAN_GETTER(CompletedGetter) const {
            EosMethodReturnValue(NanNew<Boolean>(completed_));
        }
        
        NAN_GETTER(SyncGetter) const {
            EosMethodReturnValue(NanNew<Boolean>(sync_));
        }
#endif

        Operation() 
            : completed_(false)
            , begun_(false)
            , sync_(false)
            , result_(666)
            , ownerPtr_(nullptr) 
        { 
            EOS_DEBUG_METHOD();
            
            DEBUG_ONLY(numberOfConstructedOperations++);
        }
        
        ~Operation() { 
            EOS_DEBUG_METHOD();

            DEBUG_ONLY(numberOfDestructedOperations++);

            ownerPtr_ = nullptr;
            assert(begun_ && completed_);
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
                EOS_DEBUG(L"Warning: %hs called, but args.IsConstructCall() is false\n", __FUNCTION__);

                DEBUG_ONLY(PrintStackTrace());

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

        TOwner* Owner() { 
            assert(ownerPtr_);
            return ownerPtr_; 
        }

#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
        // Returns true if the operation completed synchronously.
        bool BeginAsync() {
            EOS_DEBUG_METHOD();
            
            Begin();

            result_ = CallOverride();

            // As explained by the MSDN documentation, enabling asynchronous
            // notifications on a connection handle always succeeds if it is 
            // not connected, because it doesn't know which driver it will be
            // using. If the driver does not support asynchronous operations, 
            // it will return SQL_ERROR with SQLSTATE S1118.
            if (result_ == SQL_ERROR) {
	        SQLWCHAR state[6];

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

                if (diagRet == SQL_SUCCESS && sqlwcscmp(state, "S1118") == 0) {
                    Owner()->DisableAsynchronousNotifications();
                    
                    // Now try again. If it fails again for the same reason,
                    // pass the error back to JS.
                    result_ = CallOverride();
                }
            }

            EOS_DEBUG(L"Immediate Result: %hi\n", result_);
            
            if (result_ == SQL_STILL_EXECUTING)
                return false; // Asynchronous
        
            sync_ = true;
            DEBUG_ONLY(numberOfSyncOperations++);

            Complete();

            return true; // Synchronous
        }

        // Completed using asynchronous notifications
        void OnCompletedAsync() {
            EOS_DEBUG_METHOD_FMT(L"owner: 0x%p, operation: 0x%p", Owner(), this);

            auto retCA = SQLCompleteAsync(
                TOwner::HandleType, 
                Owner()->GetHandle(), 
                &result_);

            assert(SQL_SUCCEEDED(retCA) && "OnCompleted() called before operation actually completed");

            Complete();
        }
#endif

        void RunOnThreadPool() {
            EOS_DEBUG_METHOD();

            Begin();

            new(&req_) uv_work_t;
            req_.data = this;

            uv_queue_work(
                uv_default_loop(),
                &req_,
                &UVWorkCallback,
                &UVCompletedCallback);
        }

    protected:
        const char* GetName() const {
            return TOp::Name();
        }
        
        // Code common to beginning both Async and Thread Pool operations.
        void Begin() {
            assert(!begun_ && !completed_);

            begun_ = true;

            DEBUG_ONLY(numberOfBegunOperations++);
            DEBUG_ONLY(activeOperations_.push_back(this));

            Owner()->Ref();
            this->Ref();
        }

        // Default implementation.
        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();
            
            if (!SQL_SUCCEEDED(ret))
                return CallbackErrorOverride(ret);
            
            MakeCallback(0, nullptr);
        }

        void CallbackErrorOverride(SQLRETURN ret) {
            Handle<Value> argv[] = { Owner()->GetLastError() };
            MakeCallback(argv);
        }

        void MakeCallback(int argc, Handle<Value>* argv) {
            auto cb = GetCallback();
            Owner()->Unref();
            Unref();
            NanMakeCallback(NanGetCurrentContext()->Global(), cb, argc, argv);
        }

        // Convenience overload
        template <size_t argc>
        void MakeCallback(Handle<Value> (&argv)[argc]) {
            MakeCallback(argc, argv);
        }

        // Removes from the active operations list, and performs the callback, catching
        // any errors (and raising as fatal exceptions).
        void Complete() {
            assert(begun_ && !completed_);
            completed_ = true;

#if defined(DEBUG)
            numberOfCompletedOperations++;

            auto n0 = activeOperations_.size();
            auto end = remove(activeOperations_.begin(), activeOperations_.end(), this);
            activeOperations_.erase(end, activeOperations_.end());
            auto n1 = activeOperations_.size();
            assert(n1 < n0);
            assert(n1 == n0 - 1);
            assert(find(activeOperations_.cbegin(), activeOperations_.cend(), this) 
                == activeOperations_.cend()); 
#endif

            TryCatch tc;
            CallbackOverride(result_);
            if (tc.HasCaught())
                FatalException(tc);
        }

#pragma region UV thread pool code
        static void UVWorkCallback(uv_work_t* req) {
            auto op = static_cast<Operation<TOwner, TOp>*>(req->data);
            assert(op && &op->req_ == req);

            op->result_ = op->CallOverride();
        }

        static void UVCompletedCallback(uv_work_t* req, int status) {
            auto op = static_cast<Operation<TOwner, TOp>*>(req->data);
            assert(op && &op->req_ == req);
            assert(op->begun_ && !op->completed_);

            NanScope();

            op->Complete();
        }
#pragma endregion 

    private:
        Operation(const Operation<TOwner, TOp>& other); // = delete

        // For thread pool tasks
        uv_work_t req_;
        SQLRETURN result_;

        bool completed_, begun_, sync_;
        TOwner* ownerPtr_;
        Persistent<Object> owner_;
        static Persistent<FunctionTemplate> constructor_;
    };
}
