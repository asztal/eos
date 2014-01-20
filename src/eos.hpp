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
#define EOS_DEBUG_METHOD() printf(" -> %s\n", __FUNCTION__ ## "()")
#else
#define EOS_DEBUG(...) void(0)
#define EOS_DEBUG_METHOD() void(0)
#endif

#if defined(UNICODE)
#endif

namespace Eos {
    struct ClassInitializerRecord {
        void (*Init)(Handle<Object> exports);
        ClassInitializerRecord* next;
    };

    void RegisterClassInitializer(ClassInitializerRecord& newRecord);

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

    template <SQLSMALLINT HandleType>
    struct ODBCHandle {
        ODBCHandle(const SQLHANDLE handle) 
            : handle_(handle)
        { }

        ~ODBCHandle() {
            SQLFreeHandle(HandleType, handle_);
            handle_ = SQL_NULL_HANDLE;
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
    
    Local<Value> OdbcError(Handle<String> message);
    Local<Value> OdbcError(Handle<String> message, Handle<String> state);
    Local<Value> GetLastError(SQLSMALLINT handleType, SQLHANDLE handle);

    template<SQLSMALLINT HandleType>
    Local<Value> GetLastError(const ODBCHandle<HandleType>& handle) {
        return GetLastError(HandleType, handle.Value());
    }

    Local<String> StringFromTChar(const SQLWCHAR* string);
}

