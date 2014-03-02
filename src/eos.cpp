#include "eos.hpp"

#include "uv.h"
#include <ctime>
#include <climits>

int EosMethodDebugger::depth = 0;

namespace Eos {
    ClassInitializerRecord rootClassInitializer = { 0 };

    void RegisterClassInitializer(ClassInitializerRecord& newRecord) {
        auto rec = &rootClassInitializer;
        while (rec->next) 
            rec = rec->next; 
        rec->next = &newRecord;
    }

    namespace {
        void InitError(Handle<Object> exports);
    }

    void Init(Handle<Object> exports) {
        EOS_DEBUG_METHOD();

        auto rec = &rootClassInitializer;
        while (rec = rec->next) {
            if (rec->Init)
                rec->Init(exports);
        }

        NODE_DEFINE_CONSTANT(exports, SQL_PARAM_INPUT);
        NODE_DEFINE_CONSTANT(exports, SQL_PARAM_INPUT_OUTPUT);
        NODE_DEFINE_CONSTANT(exports, SQL_PARAM_INPUT_OUTPUT_STREAM);
        NODE_DEFINE_CONSTANT(exports, SQL_PARAM_OUTPUT);
        NODE_DEFINE_CONSTANT(exports, SQL_PARAM_OUTPUT_STREAM);
        
        NODE_DEFINE_CONSTANT(exports, SQL_CHAR);
        NODE_DEFINE_CONSTANT(exports, SQL_VARCHAR);
        NODE_DEFINE_CONSTANT(exports, SQL_LONGVARCHAR);
        NODE_DEFINE_CONSTANT(exports, SQL_WCHAR);
        NODE_DEFINE_CONSTANT(exports, SQL_WVARCHAR);
        NODE_DEFINE_CONSTANT(exports, SQL_WLONGVARCHAR);
        NODE_DEFINE_CONSTANT(exports, SQL_DECIMAL);
        NODE_DEFINE_CONSTANT(exports, SQL_NUMERIC);
        NODE_DEFINE_CONSTANT(exports, SQL_BIT);
        NODE_DEFINE_CONSTANT(exports, SQL_TINYINT);
        NODE_DEFINE_CONSTANT(exports, SQL_SMALLINT);
        NODE_DEFINE_CONSTANT(exports, SQL_INTEGER);
        NODE_DEFINE_CONSTANT(exports, SQL_BIGINT);
        NODE_DEFINE_CONSTANT(exports, SQL_REAL);
        NODE_DEFINE_CONSTANT(exports, SQL_FLOAT);
        NODE_DEFINE_CONSTANT(exports, SQL_DOUBLE);
        NODE_DEFINE_CONSTANT(exports, SQL_BINARY);
        NODE_DEFINE_CONSTANT(exports, SQL_VARBINARY);
        NODE_DEFINE_CONSTANT(exports, SQL_LONGVARBINARY);
        NODE_DEFINE_CONSTANT(exports, SQL_TYPE_DATE);
        NODE_DEFINE_CONSTANT(exports, SQL_TYPE_TIME);
        NODE_DEFINE_CONSTANT(exports, SQL_TYPE_TIMESTAMP);
        //NODE_DEFINE_CONSTANT(exports, SQL_INTERVAL);
        NODE_DEFINE_CONSTANT(exports, SQL_GUID);

        InitError(exports);
    }

#pragma region("Wait")
    namespace Async {
        int initCount = 0;
        uv_async_t async;
        uv_rwlock_t rwlock;
        
        struct Callback {
            INotify* target;
            Callback* next;

            Callback(INotify* target) 
                : target(target)
                , next(0) 
            { }
        } *firstCallback = 0;

        void ProcessCallbackQueue(uv_async_t*, int);

        void InitialiseWaiter() {
            if (initCount == 0) {
                EOS_DEBUG_METHOD();
                uv_async_init(uv_default_loop(), &async, &ProcessCallbackQueue);
                uv_rwlock_init(&rwlock);
            } 

            initCount++;
        }

        void DestroyWaiter() {
            initCount--;

            if (initCount == 0) {
                EOS_DEBUG_METHOD();
                uv_rwlock_destroy(&rwlock);
                uv_close(reinterpret_cast<uv_handle_t*>(&async), nullptr);
            }
        }

        void PrintCallbackChain(const wchar_t* msg) {
            assert(msg);

            if (!firstCallback) {
                EOS_DEBUG(L"%ls, callback chain is empty\n", msg);
                return;
            }

            {
                int len = 1;
                auto cb = firstCallback;
                while (cb = cb->next)
                    len++;

                EOS_DEBUG(L"%ls, chain length: %i\n", msg, len);

                cb = firstCallback;
                do {
                    cb->target;
                    EOS_DEBUG(L" ** 0x%p->Notify()\n", cb->target);
                } while (cb = cb->next);
            }
        }

        void ProcessCallbackQueue(uv_async_t*, int) {
            EOS_DEBUG_METHOD();

            uv_rwlock_rdlock(&rwlock);

            assert(firstCallback);

#if defined(DEBUG)
            PrintCallbackChain(L"Processing callbacks");
#endif 

            while (firstCallback) {
                assert(firstCallback->target);
                
                // Helps debugging... globals don't seem to be accessible when debugging
                auto fc = firstCallback; 
                auto ic = initCount;
                
                HANDLE hWait = firstCallback->target->GetWaitHandle();

                if (UnregisterWait(hWait))
                    DestroyWaiter();
                else 
                    EOS_DEBUG(L"UnregisterWait failed\n");
                
                auto current = firstCallback;
                firstCallback = firstCallback->next;

                // If initCount is zero, the last callback must have been called,
                // so firstCallback->next should be nullptr, and we don't need to
                // lock the lock since it will have been destroyed
                if (!initCount) {
                    assert(!firstCallback && "*Must* have been the last callback in the queue if initCount is 0");
                    current->target->Notify();
                    current->target->Unref();
                    delete current;
                    return; 
                }

                uv_rwlock_rdunlock(&rwlock);
                current->target->Notify();
                current->target->Unref();
                uv_rwlock_rdlock(&rwlock);

                delete current;
            }
            
            assert(initCount);
            uv_rwlock_rdunlock(&rwlock);
        }

        void NTAPI WaitCallback(PVOID data, BOOLEAN timeout) {
            EOS_DEBUG_METHOD();

            if (timeout)
                return;

            uv_rwlock_wrlock(&rwlock);
            
            auto cb = new Callback(reinterpret_cast<INotify*>(data));

#if defined(DEBUG)
            EOS_DEBUG(L"Callback to be added: 0x%p->Notify()\n", cb->target);
            PrintCallbackChain(L"About to add new callback");
#endif 

            // Enqueue the callbacks, in the order that they happened.
            if (firstCallback) {
                auto node = firstCallback;
                while (node->next)
                    node = node->next;
                node->next = cb;
            } else {
                firstCallback = cb;
            }
            
#if defined(DEBUG)
            PrintCallbackChain(L"Added new callback");
#endif 
            uv_rwlock_wrunlock(&rwlock);

            uv_async_send(&async);
        }
    }

    HANDLE Wait(INotify* notify) {
        EOS_DEBUG_METHOD();
        Async::InitialiseWaiter();

        HANDLE hRegisteredWaitHandle = nullptr;
        if (RegisterWaitForSingleObject(&hRegisteredWaitHandle, notify->GetEventHandle(), &Async::WaitCallback, notify, INFINITE, WT_EXECUTEDEFAULT))
            notify->Ref();
        else
            EOS_DEBUG(L"RegisterWaitForSingleObject failed\n");
        
        return hRegisteredWaitHandle;
    }

#pragma endregion
    void PrintStackTrace() {
        PrintStackTrace(StackTrace::CurrentStackTrace(10));
    }

    void PrintStackTrace(Handle<StackTrace> stackTrace) {
        assert(!stackTrace.IsEmpty());

        auto frames = stackTrace->GetFrameCount();
        for (int i = 0; i < frames; ++i) {
            auto frame = stackTrace->GetFrame(i);

            WStringValue file(frame->GetScriptNameOrSourceURL());
            assert(*file);

            int line = frame->GetLineNumber(), column = frame->GetColumn();

            fwprintf(stderr, L"  at %ls:%i:%i\n", *file, line, column);
        }
    }

#pragma region("OdbcError")
    namespace {
        Persistent<Function> odbcErrorConstructor;
    
        void InitError(Handle<Object> exports) {
            EOS_DEBUG_METHOD();

            HandleScope scope;

            // http://stackoverflow.com/a/17936621/1794628
            auto code = 
            "function OdbcError(message, state, subErrors) {\
                if (!(this instanceof OdbcError))\
                    return new OdbcError(message, state, subErrors);\
                var e = Error.call(this, message);\
                e.name = this.name = 'OdbcError';\
                this.stack = e.stack; \
                this.message = e.message; \
                if (state) \
                    this.state = state;\
                if (subErrors)\
                    this.errors = subErrors;\
                return this;\
            }\
            var II = function() {};\
            II.prototype = Error.prototype;\
            OdbcError.prototype = new II();\
            OdbcError";

            odbcErrorConstructor = Persistent<Function>::New(Handle<Function>::Cast(Script::Compile(String::New(code))->Run()));

            exports->Set(String::NewSymbol("OdbcError"), odbcErrorConstructor, ReadOnly);
        }
    }

    Local<Value> OdbcError(Handle<String> message) {
        assert(!odbcErrorConstructor.IsEmpty());
        assert(!message.IsEmpty());

        Handle<Value> args[] = { message };
        return odbcErrorConstructor->CallAsConstructor(1, args);
    }

    Local<Value> OdbcError(const char* message) {
        return OdbcError(String::New(message));
    }

    Local<Value> OdbcError(const wchar_t* message) {
        return OdbcError(String::New(reinterpret_cast<const uint16_t*>(message)));
    }

    Local<Value> OdbcError(Handle<String> message, Handle<String> state) {
        assert(!odbcErrorConstructor.IsEmpty());
        assert(!message.IsEmpty());
        assert(!state.IsEmpty());

        Handle<Value> args[] = { message, state };
        return odbcErrorConstructor->CallAsConstructor(2, args);
    }

    Local<Value> OdbcError(Handle<String> message, Handle<String> state, Handle<Array> subErrors) {
        assert(!odbcErrorConstructor.IsEmpty());
        assert(!message.IsEmpty());
        assert(!state.IsEmpty());
        assert(!subErrors.IsEmpty());

        Handle<Value> args[] = { message, state, subErrors };
        return odbcErrorConstructor->CallAsConstructor(3, args);
    }
#pragma endregion
    
    Handle<Value> ThrowError(const char* message) {
        return ThrowException(Exception::Error(String::New(message)));
    }

    Handle<Value> ThrowTypeError(const char* message) {
        return ThrowException(Exception::TypeError(String::New(message)));
    }

    Local<Value> GetLastError(SQLSMALLINT handleType, SQLHANDLE handle) {
        EOS_DEBUG_METHOD();

        HandleScope scope;

        assert(handleType == SQL_HANDLE_ENV || handleType == SQL_HANDLE_DBC || handleType == SQL_HANDLE_STMT || handleType == SQL_HANDLE_STMT || handleType == SQL_HANDLE_DESC);
        assert(handle != SQL_NULL_HANDLE);

        SQLINTEGER nFields;
        auto ret = SQLGetDiagFieldW(
            handleType,
            handle,
            0,
            SQL_DIAG_NUMBER,
            &nFields,
            SQL_IS_INTEGER,
            nullptr);

        if (!SQL_SUCCEEDED(ret))
            return scope.Close(Exception::Error(String::New("Unknown ODBC error (error calling SQLGetDiagField)")));

        if (nFields == 0) {
            EOS_DEBUG(L"No error to return!\n");
            return scope.Close(Null());
        }

        assert(nFields >= 0 && nFields < INT_MAX);
        Local<Array> errors = Array::New(nFields);
        Local<String> resultMessage, resultState;
        Local<Object> result;

        for (SQLINTEGER i = 0; i < nFields; i++) {
            SQLWCHAR state[5 + 1] = L"", message[1024 + 1] = L"";
            SQLSMALLINT messageLength;

            auto ret = SQLGetDiagRecW(
                handleType,
                handle, 
                static_cast<SQLSMALLINT>(i + 1), // Shut up cast warning
                state, 
                nullptr,
                message, 
                static_cast<SQLSMALLINT>(sizeof(message) / sizeof(message[0])),
                &messageLength);

            Local<Value> item;

            Local<String> msg, st;
            if (SQL_SUCCEEDED(ret))
                item = OdbcError(msg = StringFromTChar(message), st = StringFromTChar(state));
            else if(ret == SQL_INVALID_HANDLE)
                item = OdbcError(msg = String::New("Error calling SQLGetDiagRec: SQL_INVALID_HANDLE"));
            else
                item = OdbcError(msg = String::New("Unknown ODBC error"));

            if (i == 0) {
                resultMessage = msg;
                resultState = st;
            }

            errors->Set(i, item);
        }

        if (!resultState.IsEmpty())
            return scope.Close(OdbcError(resultMessage, resultState, errors));
        else
            return scope.Close(OdbcError(resultMessage));
    }

    Local<String> StringFromTChar(const SQLWCHAR* string, int length) {
        HandleScope scope;
        return scope.Close(String::New(reinterpret_cast<const uint16_t*>(string), length));
    }

    
    SQLSMALLINT GetCTypeForSQLType(SQLSMALLINT sqlType) {
        switch(sqlType) {
            case SQL_INTEGER: case SQL_SMALLINT: case SQL_TINYINT:
                return SQL_C_SLONG;
    
            case SQL_NUMERIC: case SQL_DECIMAL: case SQL_BIGINT:
            case SQL_FLOAT: case SQL_REAL: case SQL_DOUBLE:
                return SQL_C_DOUBLE;

            case SQL_DATETIME: case SQL_TIMESTAMP:
                return SQL_C_TYPE_TIMESTAMP;

            case SQL_BINARY: case SQL_VARBINARY: case SQL_LONGVARBINARY:
                return SQL_C_BINARY;

            case SQL_BIT:
                return SQL_C_BIT;

            default:
                return SQL_C_TCHAR;
        }
    }

    SQLSMALLINT GetSQLType(Handle<Value> jsValue) {
        if (jsValue->IsInt32())
            return SQL_INTEGER;
        if (jsValue->IsNumber())
            return SQL_DOUBLE;
        if (jsValue->IsBoolean())
            return SQL_BIT;
        if (jsValue->IsDate())
            return SQL_TIMESTAMP;
        if (Buffer::HasInstance(jsValue))
            return SQL_LONGVARBINARY;
        if (jsValue->IsObject() && JSBuffer::HasInstance(jsValue.As<Object>()))
            return SQL_LONGVARBINARY;

        return SQL_WCHAR;
    }

    Handle<Value> ConvertToJS(SQLPOINTER buffer, SQLLEN bufferLength, SQLSMALLINT cType) {
        switch(cType) {
        case SQL_C_SLONG:
            return Number::New(*reinterpret_cast<long*>(buffer));

        case SQL_C_DOUBLE:
            return Number::New(*reinterpret_cast<double*>(buffer));
        
        case SQL_C_BIT:
            return *reinterpret_cast<bool*>(buffer) ? True() : False();

        case SQL_C_CHAR:
            return String::New(reinterpret_cast<const char*>(buffer), bufferLength);

        case SQL_C_WCHAR:
            assert(bufferLength % 2 == 0);
            return StringFromTChar(reinterpret_cast<const SQLWCHAR*>(buffer), bufferLength / 2);

        case SQL_C_TYPE_TIMESTAMP: {
            SQL_TIMESTAMP_STRUCT& ts = *reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(buffer);
            tm tm = { 0 };
            tm.tm_year = ts.year - 1900;
            tm.tm_mon = ts.month - 1;
            tm.tm_mday = ts.day;
            tm.tm_hour = ts.hour;
            tm.tm_min = ts.minute;
            tm.tm_sec = ts.second;
            
#if defined(WIN32)
        return Date::New((double(mktime(&tm)) * 1000)
               + (ts.fraction / 1000000.0));
#else
        return Date::New((double(timelocal(&tm)) * 1000)
               + (ts.fraction / 1000000.0));
#endif
        }

        default:
            return Undefined();
        }
    }

    void JSBuffer::Init(Handle<Object>) {
        auto val = Context::GetCurrent()->Global()->Get(String::NewSymbol("Buffer"));
        if (!val->IsFunction())
            ThrowError("The global Buffer object is not a function. Please don't duck punch the Buffer object when using Eos.");

        constructor_ = Persistent<Function>::New(val.As<Function>());
    }

    Handle<Object> JSBuffer::New(Handle<String> string, Handle<String> enc) {
        Handle<Value> argv[] = { string, enc };
        return Constructor()->NewInstance(2, argv);
    }

    bool JSBuffer::HasInstance(Handle<Value> value) {
        return value->IsObject() 
            && HasInstance(value.As<Object>());
    }
    
    bool JSBuffer::HasInstance(Handle<Object> value) {
        return value->GetConstructor()->StrictEquals(JSBuffer::Constructor());
    }

    Handle<Object> JSBuffer::New(size_t length) {
        assert(length <= UINT32_MAX);

        Handle<Value> argv[] = { Uint32::New(length) };
        return Constructor()->NewInstance(1, argv);
    }

    const char* JSBuffer::Unwrap(Handle<Object> handle, SQLPOINTER& buffer, SQLLEN& bufferLength) {
        if (Buffer::HasInstance(handle)) {
            buffer = Buffer::Data(handle);
            bufferLength = Buffer::Length(handle);
            return nullptr;
        }

        auto parent = handle->Get(String::NewSymbol("parent"));
        auto sliceOffset = handle->Get(String::NewSymbol("offset"))->Int32Value();
        auto sliceLength = handle->Get(String::NewSymbol("length"))->Int32Value();
                
        if (!parent->IsObject() || !Buffer::constructor_template->HasInstance(parent))
            return "Invalid Buffer object (parent is not a SlowBuffer)";
        Buffer* slow = ObjectWrap::Unwrap<Buffer>(parent.As<Object>());

        buffer = Buffer::Data(slow);
        bufferLength = Buffer::Length(slow);
                
        if (sliceOffset < 0 || sliceLength < 0 || sliceOffset + sliceLength > bufferLength)
            return "Invalid Buffer object (offset/length invalid)";

        buffer = reinterpret_cast<char*>(buffer) + sliceOffset;
        bufferLength = sliceLength;

        return nullptr;
    }

    // The first parameter might actually be a buffer, might be a SlowBuffer...
    // Doesn't really matter.
    Handle<Object> JSBuffer::Slice(Handle<Object> buffer, SQLLEN offset, SQLLEN length) {
        auto slice = buffer->Get(String::NewSymbol("slice"));
        assert (slice->IsFunction() && "The passed buffer object does not have a 'slice' function");

        auto sliceFn = Local<Function>::Cast(slice);
        Local<Value> argv[] = { Integer::New(offset), Integer::New(length) };
        auto result = sliceFn->Call(buffer, 2, argv);

        assert(result->IsObject() && "The result of slice() is not an object");
        return result->ToObject();
    }

    Persistent<Function> JSBuffer::constructor_;
    ClassInitializer<JSBuffer> jsBufferInit;
}

NODE_MODULE(eos, &Eos::Init)
