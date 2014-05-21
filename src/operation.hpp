#pragma once 

#include "eos.hpp"

namespace Eos {
    struct IOperation : ObjectWrap {
        IOperation() {
            EOS_DEBUG_METHOD();
        
#if defined(DEBUG)
            NanAssignPersistent(stackTrace_, 
                IF_NODE_12
                    ( StackTrace::CurrentStackTrace(nan_isolate, 10)
                    , NanNew<StackTrace>(10))
                );
#endif
        }

        virtual ~IOperation() {
            EOS_DEBUG_METHOD();
        }

        // Returns true if the operation completed synchronously.
        bool Begin(bool isComplete = false) {
            EOS_DEBUG_METHOD();
            
            OnBegin();

            auto ret = Call();

            EOS_DEBUG(L"Immediate Result: %hi\n", ret);
            
            if (ret == SQL_STILL_EXECUTING) {
                assert(!isComplete);
                return false; // Asynchronous
            }
        
            Callback(ret);
            return true; // Synchronous
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
            callback_->Call(Context::GetCurrent()->Global(), argc, argv);
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

            auto obj = TOp::New(owner, args);
            if (obj->IsUndefined())
                return obj;

            assert(obj->IsObject());

            TOp* op = ObjectWrap::Unwrap<TOp>(obj->ToObject());
            op->owner_ = owner->handle_;
            NanAssignPersistent(op->callback_, NanNew<Function>(args[args.Length() - 1].As<Function>()));

            return obj;
        };

        static Handle<FunctionTemplate> Constructor() {
            return constructor_;
        }

        TOwner* Owner() { return ObjectWrap::Unwrap<TOwner>(NanNew(owner_)); }

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

    protected:
        void CallbackErrorOverride(SQLRETURN ret) {
            Handle<Value> argv[] = { Owner()->GetLastError() };
            GetCallback()->Call(NanGetCurrentContext()->Global(), 1, argv);
        }

    private:
        void OnBegin() {
            EOS_DEBUG_METHOD();
            Owner()->Ref();
            this->Ref();
        }

        bool completed_;
        Persistent<Object> owner_;
        static Persistent<FunctionTemplate> constructor_;
    };
}
