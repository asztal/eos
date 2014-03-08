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
// Note about %s: Microsoft Visual C++ and GNU libc treat %s differently in wprintf.
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

    struct INotify {
        virtual void Notify() = 0;
        virtual HANDLE GetEventHandle() const = 0;
        virtual HANDLE GetWaitHandle() const = 0;

        virtual void Ref() = 0;
        virtual void Unref() = 0;
    };

    HANDLE Wait(INotify* target);
    
    void PrintStackTrace();
    void PrintStackTrace(Handle<StackTrace>);

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

    template <class T, Handle<Value> (T::* F)() const> 
    struct GetterWrapper {
        static Handle<Value> Fun(Local<String> property, const AccessorInfo& info) {
            HandleScope scope;

            auto holder = info.Holder();
            if (!T::Constructor()->HasInstance(holder))
                return ThrowError(__FUNCTION__ ": Getter called on the wrong type of object");

            T* obj = ObjectWrap::Unwrap<T>(holder);
            return scope.Close((obj->*F)());
        }
    };

    template <class T, void (T::*F)(Local<Value>)> 
    struct SetterWrapper {
        static void Fun(Local<String> property, Local<Value> value, const AccessorInfo& info) {
            HandleScope scope;

            auto holder = info.Holder();
            if (!T::Constructor()->HasInstance(holder))
                // I don't actually know what happens if one throws here
                ThrowError(__FUNCTION__ ": Setter called on the wrong type of object");
            else
                (ObjectWrap::Unwrap<T>(holder)->*F)(value);
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
        
        static bool HasInstance(Handle<Value> value);
        static bool HasInstance(Handle<Object> value);

        static Handle<Object> New(Handle<String> str);
        static Handle<Object> New(Handle<String> str, Handle<String> enc);

        static Handle<Object> New(Buffer* slowBuffer, size_t newLength = 0);
        static Handle<Object> New(size_t length);
        static Handle<Function> Constructor() { return constructor_; }
        static Handle<Object> Slice(Handle<Object> jsBuffer, SQLLEN offset, SQLLEN length);
        static const char* Unwrap(Handle<Object> jsBuffer, SQLPOINTER& buffer, SQLLEN& length);

    private:
        static Persistent<Function> constructor_;
    };

    SQLSMALLINT GetSQLType(Handle<Value> jsValue);
    SQLSMALLINT GetCTypeForSQLType(SQLSMALLINT sqlType);
    Handle<Value> ConvertToJS(SQLPOINTER buffer, SQLLEN bufferLength, SQLSMALLINT targetCType);
    
    template <typename T>
    inline Persistent<T> Persist(Handle<T> value) {
        return Persistent<T>::New(value);
    }

#define EOS_SET_METHOD(target, name, type, method, sig) ::Eos::SetPrototypeMethod(target, name, &::Eos::Wrapper<type, &type::method>::Fun, sig)
#define EOS_SET_GETTER(target, name, type, method) target->InstanceTemplate()->SetAccessor(String::NewSymbol(name), &::Eos::GetterWrapper<type, &type::method>::Fun)
#define EOS_SET_ACCESSOR(target, name, type, getter, setter) target->InstanceTemplate()->SetAccessor(String::NewSymbol(name), &::Eos::GetterWrapper<type, &type::getter>::Fun, &::Eos::SetterWrapper<type, &type::setter>::Fun)

    template <typename T> T min (const T& x, const T& y) { return x < y ? x : y; }
    template <typename T> T max (const T& x, const T& y) { return x > y ? x : y; }

    struct Environment;
    struct Connection;
    struct Statement;
}

