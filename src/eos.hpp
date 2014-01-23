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

#if defined(DEBUG)
#define EOS_DEBUG(...) wprintf(__VA_ARGS__)
#define EOS_DEBUG_METHOD() wprintf(L" -> %s()\n", __FUNCTIONW__)
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

    template <class TOp>
    struct Operation : ObjectWrap {
        static void Init(Handle<Object> exports) {
            EOS_DEBUG_METHOD();
        }

    private:
        Persistent<Function> callback_;

        static ClassInitializer<Operation<TOp> > classInitializer_;
    };

    struct Connect : Operation<Connect> {

    private:
    };
    
    // Create an OdbcError with no SQLSTATE or sub-errors.
    Local<Value> OdbcError(Handle<String> message);
    Local<Value> OdbcError(const char* message);
    Local<Value> OdbcError(const wchar_t* message);

    // Create an OdbcError with an SQLSTATE but no sub-errors.
    Local<Value> OdbcError(Handle<String> message, Handle<String> state);

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
    Local<String> StringFromTChar(const SQLWCHAR* string);

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
    
#define EOS_SET_METHOD(target, name, type, method, sig) ::Eos::SetPrototypeMethod(target, name, &::Eos::Wrapper<type, &type::method>::Fun, sig)
}

