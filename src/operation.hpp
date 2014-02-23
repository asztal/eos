#pragma once 

#include "eos.hpp"

namespace Eos {
    struct IOperation : ObjectWrap {
        IOperation() {
            EOS_DEBUG_METHOD();
        
#if defined(DEBUG)
            stackTrace_ = Persistent<StackTrace>::New(StackTrace::CurrentStackTrace(10));
#endif
        }

        virtual ~IOperation() {
            EOS_DEBUG_METHOD();
        }

        // Returns true if the operation completed synchronously.
        bool Begin(bool isComplete = false) {
            EOS_DEBUG_METHOD();
            
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
        
        virtual void CallbackOverride(SQLRETURN ret) {
            // Default implementation.
            EOS_DEBUG_METHOD();
            
            if (!SQL_SUCCEEDED(ret))
                return CallbackErrorOverride(ret);

            GetCallback()->Call(Context::GetCurrent()->Global(), 0, nullptr);
        }
        
        virtual void CallbackErrorOverride(SQLRETURN ret) = 0;

        Handle<Function> GetCallback() const {
            return callback_;
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

            constructor_ = Persistent<FunctionTemplate>::New(FunctionTemplate::New(New));
            constructor_->SetClassName(String::NewSymbol(TOp::Name()));
            constructor_->InstanceTemplate()->SetInternalFieldCount(1);
        }

        Operation() { 
            EOS_DEBUG_METHOD();
        }

        template<size_t argc>
        static Local<Object> Construct(Handle<Value> (&argv)[argc]) {
            return Constructor()->GetFunction()->NewInstance(argc, argv); 
        }

        static Handle<Value> New(const Arguments& args) {
            EOS_DEBUG_METHOD();
            
            HandleScope scope;

            if (args.Length() < 2)
                return scope.Close(ThrowError("Too few arguments"));

            if (!args.IsConstructCall()) {
                EOS_DEBUG(L"Warning: %ls called, but args.IsConstructCall() is false\n", __FUNCTIONW__);
#if defined(DEBUG)
                PrintStackTrace();
#endif
                // XXX There must be a better way...
                Handle<Value>* argv = new Handle<Value>[args.Length()];
                for (int i = 0; i < args.Length(); i++)
                    argv[i] = args[i];
                auto ret = constructor_->GetFunction()->NewInstance(args.Length(), argv);
                delete[] argv;
                return ret;
            }

            if (!TOwner::Constructor()->HasInstance(args[0]))
                return scope.Close(ThrowTypeError("Bad argument"));

            if (!args[args.Length() - 1]->IsFunction())
                return scope.Close(ThrowTypeError("Last argument should be a callback function"));

            auto owner = ObjectWrap::Unwrap<TOwner>(args[0]->ToObject());

            auto obj = TOp::New(owner, args);
            if (obj->IsUndefined())
                return obj;

            assert(obj->IsObject());

            TOp* op = ObjectWrap::Unwrap<TOp>(obj->ToObject());
            op->owner_ = owner->handle_;
            op->callback_ = Persistent<Function>::New(args[args.Length() - 1].As<Function>());
            return obj;
        };

        static Handle<FunctionTemplate> Constructor() {
            return constructor_;
        }

        TOwner* Owner() { return ObjectWrap::Unwrap<TOwner>(owner_); }

        void OnCompleted() {
            EOS_DEBUG_METHOD_FMT(L"owner: 0x%p, operation: 0x%p", Owner(), this);

            SQLRETURN ret;
            SQLCompleteAsync(TOwner::HandleType, Owner()->GetHandle(), &ret);
            
            TryCatch tc;
            CallbackOverride(ret);
            if (tc.HasCaught())
                FatalException(tc);
        }

    protected:
        void CallbackErrorOverride(SQLRETURN ret) {
            Handle<Value> argv[] = { Owner()->GetLastError() };
            GetCallback()->Call(Context::GetCurrent()->Global(), 1, argv);
        }

    private:
        Persistent<Object> owner_;
        static Persistent<FunctionTemplate> constructor_;
    };
}
