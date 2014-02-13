#pragma once 

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>

#include <new>

#include <sql.h>
#include <sqltypes.h>
#include <sqlext.h>
#include <sqlucode.h>

using namespace v8;
using namespace node; 
using std::nothrow;

#if !defined(UNICODE)
#error Please #define UNICODE.
#endif

struct EosMethodDebugger {
    EosMethodDebugger(const wchar_t* fn) {
        for (int i = 0; i < depth; i++)
            wprintf(L".");
        wprintf(L"%s()\n", fn);
        ++depth;
    }

    ~EosMethodDebugger() {
        --depth;
        assert(depth >= 0);
    }
private:
    static int depth;
};

#if defined(DEBUG)
#define EOS_DEBUG(...) wprintf(__VA_ARGS__)
#define EOS_DEBUG_METHOD() EosMethodDebugger __md__(__FUNCTIONW__); //wprintf(L" -> %s()\n", __FUNCTIONW__)
#else
#define EOS_DEBUG(...) void(0)
#define EOS_DEBUG_METHOD() void(0)
#endif

#if defined(UNICODE)
#endif

namespace Eos {
    // A linked list of initialisers to be called when the module is initialised.
    struct ClassInitializerRecord {
        void (*Init)(Handle<Object> exports);
        ClassInitializerRecord* next;
    };

    // Register a function which will be called when Eos::Init() is called.
    void RegisterClassInitializer(ClassInitializerRecord& newRecord);

    // Upon construction, registers a class initialiser which will call T::Init(exports).
    // The order of execution of class initialisers is not defined.
    template <class T>
    struct ClassInitializer {
        ClassInitializer::ClassInitializer() {
            rec.Init = &T::Init;
            rec.next = 0;
            RegisterClassInitializer(rec);
        }
    private:
        ClassInitializerRecord rec;
    };

    // SQLHANDLE wrapper to disallow assigning between different types of handles
    template <SQLSMALLINT HandleType>
    struct ODBCHandle {
        ODBCHandle(const SQLHANDLE handle) 
            : handle_(handle)
        { }

        SQLRETURN Free() {
            // Allow freeing a handle before its time (i.e. explicitly freeing
            // the object from JS.)
            SQLRETURN ret = SQL_SUCCESS;
            if (handle_ != SQL_NULL_HANDLE) {
                ret = SQLFreeHandle(HandleType, handle_);
                if (SQL_SUCCEEDED(ret))
                    handle_ = SQL_NULL_HANDLE;
            } else {
                handle_ = SQL_NULL_HANDLE;
            }
            return ret;
        }

        operator bool() const {
            return handle_ != SQL_NULL_HANDLE;
        }

        bool operator!() const {
            return handle_ == SQL_NULL_HANDLE;
        }

        friend bool operator == (ODBCHandle<HandleType> lhs, ODBCHandle<HandleType> rhs) {
            return lhs.handle_ == rhs.handle_;
        }

        friend bool operator != (ODBCHandle<HandleType> lhs, ODBCHandle<HandleType> rhs) {
            return lhs.handle_ != rhs.handle_;
        }

        ~ODBCHandle() {
            Free();
        }

        SQLHANDLE Value() const { return handle_; }

    private:
        // No copy constructor (who would free it?)
        ODBCHandle::ODBCHandle(const ODBCHandle<HandleType>& other);

        SQLHANDLE handle_;
    };

    struct IOperation : ObjectWrap {
        virtual ~IOperation() {
            EOS_DEBUG_METHOD();
        }

        void Begin(bool isComplete = false) {
            EOS_DEBUG_METHOD();
            
            auto ret = Call();

            EOS_DEBUG(L"Immediate Result: %hi\n", ret);
            
            if (ret == SQL_STILL_EXECUTING) {
                assert(!isComplete);
                return;
            }

            if (!SQL_SUCCEEDED(ret)) {
                TryCatch tc;
                CallbackErrorOverride(ret);
                if (tc.HasCaught())
                    FatalException(tc);
                return;
            }
        
            TryCatch tc;
            Callback(ret);
            if (tc.HasCaught())
                FatalException(tc);
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

    protected:
        Persistent<Function> callback_;
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
            EOS_DEBUG_METHOD();

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

    struct INotify {
        virtual void Notify() = 0;
    };

    HANDLE Wait(HANDLE hWaitHandle, INotify* target);
    void Unwait(HANDLE hRegisteredWaitHandle);

    // Create an OdbcError with no SQLSTATE or sub-errors.
    Local<Value> OdbcError(Handle<String> message);
    Local<Value> OdbcError(const char* message);
    Local<Value> OdbcError(const wchar_t* message);

    // Create an OdbcError with an SQLSTATE but no sub-errors.
    Local<Value> OdbcError(Handle<String> message, Handle<String> state);
    
    Handle<Value> ThrowError(const char* message);
    Handle<Value> ThrowTypeError(const char* message);

    // Use SQLGetDiagRecW to return an OdbcError representing the last error which happened
    // for the given handle. The first error will be returned, but if there are multiple
    // errors they will be accessible via the OdbcError.prototype.errors property.
    Local<Value> GetLastError(SQLSMALLINT handleType, SQLHANDLE handle);

    template<SQLSMALLINT HandleType>
    Local<Value> GetLastError(const ODBCHandle<HandleType>& handle) {
        return GetLastError(HandleType, handle.Value());
    }

    // Do type hackery to cast from const SQLWCHAR* to const uint16_t* to please V8's 
    // String::New function.
    Local<String> StringFromTChar(const SQLWCHAR* string, int length = -1);

    // Node's SetPrototypeMethod doesn't set a v8::Signature, which doesn't help debugging. Not
    // setting a Signature on a prototype method of an ObjectWrap class can result in segfault
    // or assertion, if the method is .call()'d on an ObjectWrap which is not the correct type,
    // e.g. Environment.prototype.connections.call({})
    inline void SetPrototypeMethod(Handle<FunctionTemplate> target, const char* name, InvocationCallback callback, Handle<Signature> sig) {
        auto ft = FunctionTemplate::New(callback, Handle<Value>(), sig);
        target->PrototypeTemplate()->Set(String::NewSymbol(name), ft);
    }

    template <class T, Handle<Value> (T::*F)(const Arguments&)> 
    struct Wrapper {
        static Handle<Value> Fun(const Arguments& args) {
            HandleScope scope;
            T* obj = ObjectWrap::Unwrap<T>(args.Holder());
            return scope.Close((obj->*F)(args));
        }
    };

    struct WStringValue {
        WStringValue(Handle<Value> value) : value(value) {}
        SQLWCHAR* operator*() { return reinterpret_cast<SQLWCHAR*>(*value); }
        const SQLWCHAR* operator*() const { return reinterpret_cast<const SQLWCHAR*>(*value); }
        int length() const { return value.length(); }
    
    private:
        // Disallow copying and assigning.
        WStringValue(const WStringValue&);
        void operator=(const WStringValue&);

        String::Value value;
    };
    
#define EOS_SET_METHOD(target, name, type, method, sig) ::Eos::SetPrototypeMethod(target, name, &::Eos::Wrapper<type, &type::method>::Fun, sig)

    template <typename T> T min (const T& x, const T& y) { return x < y ? x : y; }
    template <typename T> T max (const T& x, const T& y) { return x > y ? x : y; }

    struct Environment;
    struct Connection;
    struct Statement;
}

