#pragma once 

#include <node.h>
#include <node_object_wrap.h>
#include <node_buffer.h>
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
    EosMethodDebugger(const wchar_t* fmt, ...) {
        for (int i = 0; i < depth; i++)
            wprintf(L".");
        va_list argp;
        va_start(argp, fmt);
        vwprintf(fmt, argp);
        va_end(argp);
        ++depth;
    }

    ~EosMethodDebugger() {
        --depth;
        assert(depth >= 0);
    }
private:
    static int depth;
};

//#define DEBUG_QUIETLY

#if defined(DEBUG) && !defined(DEBUG_QUIETLY)
// Not about %s: Microsoft Visual C++ and GNU libc treat %s differently in wprintf.
// The safest method is to use %ls for wide strings, and %hs for narrow strings, and
// to avoid using %s at all.
// See http://en.chys.info/2009/06/wprintfs/ for more information.
#define EOS_DEBUG(...) wprintf(__VA_ARGS__)
#define EOS_DEBUG_METHOD(...) EosMethodDebugger __md__(__FUNCTIONW__ L"()\n"); //wprintf(L" -> %ls()\n", __FUNCTIONW__)
#define EOS_DEBUG_METHOD_FMT(fmt, ...) EosMethodDebugger __md__(__FUNCTIONW__ L"(" fmt L")\n", __VA_ARGS__); //wprintf(L" -> %ls()\n", __FUNCTIONW__)
#else
// Don't even define this in release builds!
// Should never be there.
//#define EOS_DEBUG_NOISY(...) wprintf(__VA_ARGS__)
#define EOS_DEBUG(...) void(0)
#define EOS_DEBUG_METHOD() void(0)
#define EOS_DEBUG_METHOD_FMT(...) void(0)
#endif

#if defined(DEBUG_QUIETLY)
#define EOS_DEBUG_NOISY(...) wprintf(__VA_ARGS__)
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

    struct IOperation : ObjectWrap {
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
        virtual HANDLE GetEventHandle() const = 0;
        virtual HANDLE GetWaitHandle() const = 0;

        virtual void Ref() = 0;
        virtual void Unref() = 0;
    };

    HANDLE Wait(INotify* target);

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

    struct JSBuffer {
        static void Init(Handle<Object>);

        static Handle<Object> New(Buffer* slowBuffer, size_t newLength = 0);
        static Handle<Object> New(size_t length);
        static Handle<Function> Constructor() { return constructor_; }

        static const char* Unwrap(Handle<Object> jsBuffer, SQLPOINTER& buffer, SQLLEN& length);

    private:
        static Persistent<Function> constructor_;
    };

    SQLSMALLINT GetCTypeForSQLType(SQLSMALLINT sqlType);
    Handle<Value> ConvertToJS(SQLPOINTER buffer, SQLLEN bufferLength, SQLSMALLINT targetType);
    
#define EOS_SET_METHOD(target, name, type, method, sig) ::Eos::SetPrototypeMethod(target, name, &::Eos::Wrapper<type, &type::method>::Fun, sig)

    template <typename T> T min (const T& x, const T& y) { return x < y ? x : y; }
    template <typename T> T max (const T& x, const T& y) { return x > y ? x : y; }

    struct Environment;
    struct Connection;
    struct Statement;
}

