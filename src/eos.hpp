#pragma once 

#include <nan.h>

#if NODE_MODULE_VERSION > 11
#define NODE_12
#define IF_NODE_12(x,y) x
#else
#define IF_NODE_12(x,y) y
#endif

#include <node.h>
#include <node_object_wrap.h>
#include <node_buffer.h>
#include <v8.h>

#include <new>

#define ODBCVER 0x0381

#include <sql.h>
#include <sqltypes.h>
#include <sqlext.h>
#include <sqlucode.h>
#include <cstdarg>

// unixODBC doesn't define this
#if !defined(SQL_PARAM_DATA_AVAILABLE)
#define SQL_PARAM_DATA_AVAILABLE 101
#endif 

#include "strings.hpp"

using namespace v8;
using namespace node; 
using std::nothrow;

#if !defined(UNICODE)
#error Please #define UNICODE.
#endif

struct EosMethodDebugger {
    EosMethodDebugger(const char* fn, const wchar_t* fmt, ...) {
        for (int i = 0; i < depth; i++)
            wprintf(L".");
        puts(fn);
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
#define EOS_DEBUG_METHOD(...) EosMethodDebugger __md__(__FUNCTION__, L"()\n"); //wprintf(L" -> %hs()\n", __FUNCTION__)
#define EOS_DEBUG_METHOD_FMT(fmt, ...) EosMethodDebugger __md__(__FUNCTION__, L"(" fmt L")\n", __VA_ARGS__); //wprintf(L" -> %hs()\n", __FUNCTION__)
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
        ClassInitializer() {
            rec.Init = &T::Init;
            rec.next = 0;
            RegisterClassInitializer(rec);
        }
    private:
        ClassInitializerRecord rec;
    };

    struct INotify {
        virtual void Notify() = 0;
#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
        virtual HANDLE GetEventHandle() const = 0;
        virtual HANDLE GetWaitHandle() const = 0;
#endif

        virtual void Ref() = 0;
        virtual void Unref() = 0;
    };
    
#if defined(DEBUG)
    extern int numberOfConstructedOperations;
    extern int numberOfDestructedOperations;
    extern int numberOfBegunOperations;
    extern int numberOfSyncOperations;
    extern int numberOfCompletedOperations;

    void DebugWaitCounters();
#endif

#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
    HANDLE Wait(INotify* target);
#endif
    
    void PrintStackTrace();
    void PrintStackTrace(Handle<StackTrace>);

    // Create an OdbcError with no SQLSTATE or sub-errors.
    Local<Value> OdbcError(Handle<String> message);
    Local<Value> OdbcError(const char* message);
    Local<Value> OdbcError(const wchar_t* message);

    // Create an OdbcError with an SQLSTATE but no sub-errors.
    Local<Value> OdbcError(Handle<String> message, Handle<String> state);

    // Use SQLGetDiagRecW to return an OdbcError representing the last error which happened
    // for the given handle. The first error will be returned, but if there are multiple
    // errors they will be accessible via the OdbcError.prototype.errors property.
    Handle<Value> GetLastError(SQLSMALLINT handleType, SQLHANDLE handle);

    // Do type hackery to cast from const SQLWCHAR* to const uint16_t* to please V8's 
    // String::New function.
    Local<String> StringFromTChar(const SQLWCHAR* string, int length = -1);

    // Node's SetPrototypeMethod doesn't set a v8::Signature, which doesn't help debugging. Not
    // setting a Signature on a prototype method of an ObjectWrap class can result in segfault
    // or assertion, if the method is .call()'d on an ObjectWrap which is not the correct type,
    // e.g. Environment.prototype.connections.call({})
    inline void SetPrototypeMethod(Handle<FunctionTemplate> target, const char* name, NanFunctionCallback callback, Handle<Signature> sig) {
        NanScope();

        NODE_SET_PROTOTYPE_METHOD(target, name, callback);
    }

    template <class T, _NAN_METHOD_RETURN_TYPE (T::*F)(_NAN_METHOD_ARGS_TYPE)> 
    struct Wrapper {
        static NAN_METHOD(Fun) {
            NanScope();

            T* obj = ObjectWrap::Unwrap<T>(args.Holder());

            return (obj->*F)(args);
        }
    };

    template <class T, _NAN_GETTER_RETURN_TYPE (T::* F)(Local<String>, _NAN_GETTER_ARGS) const> 
    struct GetterWrapper {
        static NAN_GETTER(Fun) {
            NanScope();
            
            auto holder = args.Holder();
            if (!T::Constructor()->HasInstance(holder))
                return NanThrowError("Getter called on the wrong type of object");

            T* obj = ObjectWrap::Unwrap<T>(holder);

            return (obj->*F)(property, args);
        }
    };

    template <class T, _NAN_SETTER_RETURN_TYPE (T::*F)(Local<String>, Local<Value>, _NAN_SETTER_ARGS_TYPE)> 
    struct SetterWrapper {
        static NAN_SETTER(Fun) {
            NanScope();

            auto holder = args.Holder();
            if (!T::Constructor()->HasInstance(holder)) {
                // I don't actually know what happens if one throws here
                NanThrowError("Setter called on the wrong type of object");
                return;
            } else {
                return (ObjectWrap::Unwrap<T>(holder)->*F)(property, value, args);
            }
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

        //static Handle<Object> New(Buffer* slowBuffer, size_t newLength = 0);
        static Handle<Object> New(size_t length);
        static Handle<Function> Constructor() { return NanNew(constructor_); }
        static Handle<Object> Slice(Handle<Object> jsBuffer, SQLLEN offset, SQLLEN length);
        static const char* Unwrap(Handle<Object> jsBuffer, SQLPOINTER& buffer, SQLLEN& length);
    private:
        static Persistent<Function> constructor_;
    };

    SQLSMALLINT GetSQLType(Handle<Value> jsValue);
    SQLSMALLINT GetCTypeForSQLType(SQLSMALLINT sqlType);
    Handle<Value> ConvertToJS(SQLPOINTER buffer, SQLLEN indicator, SQLLEN bufferLength, SQLSMALLINT targetCType);
    
    template <typename T>
    inline Persistent<T> Persist(Handle<T> value) {
        return Persistent<T>::New(value);
    }

#define EOS_SET_METHOD(target, name, type, method, sig) ::Eos::SetPrototypeMethod(NanNew(target), name, &::Eos::Wrapper<type, &type::method>::Fun, sig)
#define EOS_SET_GETTER(target, name, type, method) target->InstanceTemplate()->SetAccessor(NanSymbol(name), &::Eos::GetterWrapper<type, &type::method>::Fun)
#define EOS_SET_ACCESSOR(target, name, type, getter, setter) target->InstanceTemplate()->SetAccessor(NanSymbol(name), &::Eos::GetterWrapper<type, &type::getter>::Fun, &::Eos::SetterWrapper<type, &type::setter>::Fun)

#define EOS_OPERATION_CONSTRUCTOR(method, owner_type) Handle<Value> method(owner_type* owner, _NAN_METHOD_ARGS_TYPE args)
#define EOS_OPERATION_CONSTRUCTOR_RETURN() return NanUndefined()

#if defined(NODE_12)
#define EosMethodReturnValue(x) NanReturnValue(x)
#else
// No need for scope.Close
#define EosMethodReturnValue(x) return x
#endif

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

    template <typename T> T min (const T& x, const T& y) { return x < y ? x : y; }
    template <typename T> T max (const T& x, const T& y) { return x > y ? x : y; }

    struct Environment;
    struct Connection;
    struct Statement;
}

