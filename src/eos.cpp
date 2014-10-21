#include "eos.hpp"
#include "handle.hpp"
#include "operation.hpp"

#include "uv.h"
#include <ctime>
#include <climits>
#include <string>

int EosMethodDebugger::depth = 0;

        uv_loop_t* default_loop;

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

#if defined(DEBUG)
        exports->Set(
            NanSymbol("debugActiveOperations"), 
            NanNew<FunctionTemplate, NanFunctionCallback>(&IOperation::DebugActiveOperations)->GetFunction(), 
            (PropertyAttribute)(ReadOnly | DontDelete));
        auto x = 
        exports->Set(
            NanSymbol("activeOperations"), 
            NanNew<FunctionTemplate, NanFunctionCallback>(&IOperation::GetActiveOperations)->GetFunction(), 
            (PropertyAttribute)(ReadOnly | DontDelete));
        exports->Set(
            NanSymbol("activeHandles"), 
            NanNew<FunctionTemplate, NanFunctionCallback>(&EosHandle::GetActiveHandles)->GetFunction(), 
            (PropertyAttribute)(ReadOnly | DontDelete));
#endif

        InitError(exports);
    }

#if defined(DEBUG)
    int numberOfConstructedOperations = 0;
    int numberOfDestructedOperations = 0;
    int numberOfBegunOperations = 0;
    int numberOfSyncOperations = 0;
    int numberOfCompletedOperations = 0;
#endif

#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
#pragma region("Wait")
    namespace Async {
#if defined(DEBUG)
        int numberOfWaits = 0, numberOfCallbacks = 0;
#endif

        inline void CheckUV(int rc) {
            assert(rc == 0);
        }

        int initCount = 0;
        uv_async_t* async = nullptr;
        uv_rwlock_t rwlock;
        
        struct Callback {
            INotify* target;
            Callback* next;

            Callback(INotify* target) 
                : target(target)
                , next(0) 
            { }
        } *firstCallback = 0;
        
#ifdef NODE_12
        void ProcessCallbackQueue(uv_async_t*);
#else
        void ProcessCallbackQueue(uv_async_t*, int);
#endif

        void InitialiseWaiter() {
            EOS_DEBUG_METHOD_FMT(L"%i++", initCount);
            assert(initCount >= 0);

            if (initCount == 0) {
                EOS_DEBUG_METHOD();

                async = new uv_async_t();

                auto loop = default_loop = uv_default_loop();
                CheckUV(uv_rwlock_init(&rwlock));
                CheckUV(uv_async_init(loop, async, &ProcessCallbackQueue));
            } 

            initCount++;
        }

        void ClosedAsync(uv_handle_t* async) {
            EOS_DEBUG_METHOD();

            delete async;
            async = nullptr;
        }

        // TODO is it possible that calling uv_close(&async) while inside
        // the async wake-up callback is what is causing the event loop to
        // stick around too long?
        void DestroyWaiter() {
            EOS_DEBUG_METHOD_FMT(L"%i--", initCount);
            assert(initCount >= 1);

            initCount--;

            if (initCount == 0) {
                EOS_DEBUG_METHOD();

                uv_close(reinterpret_cast<uv_handle_t*>(async), ClosedAsync);
                uv_rwlock_destroy(&rwlock);
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

#ifdef NODE_12
        void ProcessCallbackQueue(uv_async_t*) {
#else
        void ProcessCallbackQueue(uv_async_t*, int) {
#endif
            EOS_DEBUG_METHOD();

            uv_rwlock_rdlock(&rwlock);

            // Probably, uv_async_send() was called twice, and both calls resulted in a 
            // wake up of the main event loop, even though the first wake-up cleared 
            // both of the pending callbacks.
            if (!firstCallback)
                return;
            
#if defined(DEBUG)
            assert(initCount);
            assert(firstCallback);

            PrintCallbackChain(L"Processing callbacks");
#endif 

            while (firstCallback) {
                assert(firstCallback->target);
                
#if defined(DEBUG)
                auto node = firstCallback; auto numCallbacks = 0;
                while (node) {
                    node = node->next;
                    numCallbacks++;
                }

                assert(initCount >= numCallbacks);
#endif

                HANDLE hWait = firstCallback->target->GetWaitHandle();

                auto rv = UnregisterWait(hWait);
                // ERROR_IO_PENDING simply means the wait handle couldn't be unregistered because the callback is still executing
                // http://msdn.microsoft.com/en-us/library/windows/desktop/ms686870(v=vs.85).aspx
                if (rv || ::GetLastError() == ERROR_IO_PENDING)
                    DestroyWaiter();
                else
                    EOS_DEBUG(L"UnregisterWait failed: %i\n", ::GetLastError());
                
                auto current = firstCallback;
                firstCallback = firstCallback->next;

                // If initCount is zero, the last callback must have been called,
                // so firstCallback->next should be nullptr, and we don't need to
                // lock the lock since it will have been destroyed
                if (!initCount) {
                    assert(!firstCallback && "*Must* have been the last callback in the queue if initCount is 0");
                    DEBUG_ONLY(numberOfCallbacks++);
                    current->target->Notify();
                    current->target->Unref();
                    delete current;
                    return; 
                }

                uv_rwlock_rdunlock(&rwlock);
                DEBUG_ONLY(numberOfCallbacks++);
                current->target->Notify();
                current->target->Unref();
                uv_rwlock_rdlock(&rwlock);

                delete current;
            }
            
            // We've run out of callbacks, but there are still pending operations.
            assert(initCount >= 0);
            uv_rwlock_rdunlock(&rwlock);
        }

        void NTAPI WaitCallback(PVOID data, BOOLEAN timeout) {
            EOS_DEBUG_METHOD();

            // Another bogus assert - this wait callback
            // runs on a separate thread, so querying the 
            // state of the main thread makes no sense
            // assert(!inCallback);

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
            
            assert(async);
            uv_async_send(async);
            uv_rwlock_wrunlock(&rwlock);
        }
    }

#if defined(DEBUG)
    void DebugWaitCounters() {
        EOS_DEBUG(L"\n");
        EOS_DEBUG(L"Number of Wait() calls: %i\n", Eos::Async::numberOfWaits);
        EOS_DEBUG(L"Number of callbacks: %i\n", Eos::Async::numberOfCallbacks);
        EOS_DEBUG(L"initCount: %i\n", Eos::Async::initCount);
        EOS_DEBUG(L"\n");
        EOS_DEBUG(L"Number of constructed operations: %i\n", numberOfConstructedOperations);
        EOS_DEBUG(L"Number of destructed operations: %i\n", numberOfDestructedOperations);
        EOS_DEBUG(L"Number of begun operations: %i\n", numberOfBegunOperations);
        EOS_DEBUG(L"Number of synchronous operations: %i\n", numberOfSyncOperations);
        EOS_DEBUG(L"Number of completed operations: %i\n", numberOfCompletedOperations);
        EOS_DEBUG(L"\n");
    }
#endif

    HANDLE Wait(INotify* notify) {
        EOS_DEBUG_METHOD();

        Async::InitialiseWaiter();

        HANDLE hRegisteredWaitHandle = nullptr;
        if (RegisterWaitForSingleObject(&hRegisteredWaitHandle, notify->GetEventHandle(), &Async::WaitCallback, notify, INFINITE, WT_EXECUTEONLYONCE))
            notify->Ref();
        else
            EOS_DEBUG(L"RegisterWaitForSingleObject failed\n");
       
        EOS_DEBUG(L"Eos::Wait(%i++)\n", Eos::Async::numberOfWaits);
        DEBUG_ONLY(Eos::Async::numberOfWaits++);

        return hRegisteredWaitHandle;
    }
#pragma endregion
#endif 

    void PrintStackTrace() {
        PrintStackTrace(
            IF_NODE_12( StackTrace::CurrentStackTrace(v8::Isolate::GetCurrent(), 10)
                      , StackTrace::CurrentStackTrace(10)));
    }

    void PrintStackTrace(Handle<StackTrace> stackTrace) {
        assert(!stackTrace.IsEmpty());

        auto frames = stackTrace->GetFrameCount();
        for (int i = 0; i < frames; ++i) {
            auto frame = stackTrace->GetFrame(i);

            WStringValue file(frame->GetScriptNameOrSourceURL());

	    // Work around 4-byte wchar and 2-byte SQLWCHAR on Linux
	    std::wstring wfile;
	    if (*file)
	        for(int j = 0; j < file.length(); j++)
		    wfile.push_back((*file)[j]);
	    
            int line = frame->GetLineNumber(), column = frame->GetColumn();

            fwprintf(stderr, L"  at %ls:%i:%i\n", *file ? wfile.c_str() : L"<unknown>", line, column);
        }
    }

#pragma region("OdbcError")
    namespace {
        Persistent<Function> odbcErrorConstructor;
    
        void InitError(Handle<Object> exports) {
            EOS_DEBUG_METHOD();

            NanScope();

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

            NanAssignPersistent(odbcErrorConstructor, Handle<Function>::Cast(Script::Compile(NanNew<String>(code))->Run()));

            exports->Set(NanSymbol("OdbcError"), NanNew(odbcErrorConstructor), ReadOnly);
        }
    }

    Local<Value> OdbcError(Handle<String> message) {
        assert(!odbcErrorConstructor.IsEmpty());
        assert(!message.IsEmpty());

        Handle<Value> args[] = { message };
        return NanNew(odbcErrorConstructor)->CallAsConstructor(1, args);
    }

    Local<Value> OdbcError(const char* message) {
        return OdbcError(NanNew<String>(message));
    }

    Local<Value> OdbcError(const SQLWCHAR* message) {
        return OdbcError(NanNew<String>(reinterpret_cast<const uint16_t*>(message)));
    }

    Local<Value> OdbcError(Handle<String> message, Handle<String> state) {
        assert(!odbcErrorConstructor.IsEmpty());
        assert(!message.IsEmpty());
        assert(!state.IsEmpty());

        Handle<Value> args[] = { message, state };
        return NanNew(odbcErrorConstructor)->CallAsConstructor(2, args);
    }

    Local<Value> OdbcError(Handle<String> message, Handle<String> state, Handle<Array> subErrors) {
        assert(!odbcErrorConstructor.IsEmpty());
        assert(!message.IsEmpty());
        assert(!state.IsEmpty());
        assert(!subErrors.IsEmpty());

        Handle<Value> args[] = { message, state, subErrors };
        return NanNew(odbcErrorConstructor)->CallAsConstructor(3, args);
    }
#pragma endregion
    
    // Assumes a valid handle scope exists
    Handle<Value> GetLastError(SQLSMALLINT handleType, SQLHANDLE handle) {
        EOS_DEBUG_METHOD();

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
            return NanError("Unknown ODBC error (error calling SQLGetDiagField)");

        if (nFields == 0) {
            EOS_DEBUG(L"No error to return!\n");
            return NanNull();
        }

        assert(nFields >= 0 && nFields < INT_MAX);
        auto errors = NanNew<Array>(nFields);
        Local<String> resultMessage, resultState;
        Local<Object> result;

        for (SQLINTEGER i = 0; i < nFields; i++) {
	    SQLWCHAR state[5 + 1] = {0}, message[1024 + 1] = {0};
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
                item = OdbcError(msg = NanNew<String>("Error calling SQLGetDiagRec: SQL_INVALID_HANDLE"));
            else
                item = OdbcError(msg = NanNew<String>("Unknown ODBC error"));

            if (i == 0) {
                resultMessage = msg;
                resultState = st;
            }

            errors->Set(i, item);
        }

        if (!resultState.IsEmpty())
            return OdbcError(resultMessage, resultState, errors);
        else
            return OdbcError(resultMessage);
    }

    Local<String> StringFromTChar(const SQLWCHAR* string, int length) {
        return NanNew<String>(reinterpret_cast<const uint16_t*>(string), length);
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

            case SQL_BIT:
                return SQL_C_BIT;

            case SQL_BINARY: case SQL_VARBINARY: case SQL_LONGVARBINARY:
                return SQL_C_BINARY;
                
            case SQL_CHAR: case SQL_VARCHAR: case SQL_LONGVARCHAR:
                return SQL_C_CHAR;

            case SQL_WCHAR: case SQL_WVARCHAR: case SQL_WLONGVARCHAR:
            default:
                return SQL_C_WCHAR;
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
        if (Buffer::HasInstance(jsValue) || JSBuffer::HasInstance(jsValue))
            return SQL_LONGVARBINARY;

        return SQL_WCHAR;
    }

    Handle<Value> ConvertToJS(SQLPOINTER buffer, SQLLEN indicator, SQLLEN bufferLength, SQLSMALLINT cType) {
        if (indicator == SQL_NO_TOTAL || indicator > bufferLength)
            indicator = bufferLength;
        
        switch(cType) {
        case SQL_C_SLONG:
            return NanNew<Number>(*reinterpret_cast<SQLINTEGER*>(buffer));

        case SQL_C_DOUBLE:
            return NanNew<Number>(*reinterpret_cast<SQLDOUBLE*>(buffer));
        
        case SQL_C_BIT:
            return *reinterpret_cast<SQLCHAR*>(buffer) ? NanTrue() : NanFalse();

        case SQL_C_CHAR:
            // Subtract one character iff buffer full, due to null terminator. The buffer length can be
			// zero if the string was empty.
            if (indicator > 0 && indicator == bufferLength)
                --indicator;
            return NanNew<String>(reinterpret_cast<const char*>(buffer), indicator);

        case SQL_C_WCHAR:
            // Subtract one character iff buffer full, due to null terminator. The indicator can be zero
			// if the string was empty.
            assert(indicator % sizeof(SQLWCHAR) == 0);
            if (indicator > 0 && indicator == bufferLength)
                indicator -= sizeof(SQLWCHAR);
            return StringFromTChar(reinterpret_cast<const SQLWCHAR*>(buffer), indicator / 2);

        case SQL_C_TYPE_TIMESTAMP: {
            auto& ts = *reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(buffer);
            tm tm = ::tm();
            tm.tm_year = ts.year - 1900;
            tm.tm_mon = ts.month - 1;
            tm.tm_mday = ts.day;
            tm.tm_hour = ts.hour;
            tm.tm_min = ts.minute;
            tm.tm_sec = ts.second;
            
#if defined(WIN32)
        return NanNew<Date>((double(mktime(&tm)) * 1000)
               + (ts.fraction / 1000000.0));
#else
        return NanNew<Date>((double(timelocal(&tm)) * 1000)
               + (ts.fraction / 1000000.0));
#endif
        }

        default:
            return NanUndefined();
        }
    }

    void JSBuffer::Init(Handle<Object>) {
        auto val = NanGetCurrentContext()->Global()->Get(NanSymbol("Buffer"));
        if (!val->IsFunction()) {
            NanThrowError("The global Buffer object is not a function. Please don't duck punch the Buffer object when using Eos.");
            return;
        }

        NanAssignPersistent(constructor_, val.As<Function>());
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
        assert(length <= INT_MAX);

        Handle<Value> argv[] = { NanNew<Integer>(length) };
        return Constructor()->NewInstance(1, argv);
    }

#ifdef NODE_12
    const char* JSBuffer::Unwrap(Handle<Object> handle, SQLPOINTER& buffer, SQLLEN& bufferLength) {
        if (!Buffer::HasInstance(handle))
            return "Invalid parameter (not a Buffer object)";
        
        buffer = Buffer::Data(handle);
        bufferLength = Buffer::Length(handle);
        return nullptr;
    }
#else
    const char* JSBuffer::Unwrap(Handle<Object> handle, SQLPOINTER& buffer, SQLLEN& bufferLength) {
        if (Buffer::HasInstance(handle)) {
            buffer = Buffer::Data(handle);
            bufferLength = Buffer::Length(handle);
            return nullptr;
        }

        auto parent = handle->Get(NanSymbol("parent"));
        auto sliceOffset = handle->Get(NanSymbol("offset"))->Int32Value();
        auto sliceLength = handle->Get(NanSymbol("length"))->Int32Value();
                
        if (!parent->IsObject() || !Buffer::HasInstance(parent))
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
#endif

    // The first parameter might actually be a buffer, might be a SlowBuffer...
    // Doesn't really matter.
    Handle<Object> JSBuffer::Slice(Handle<Object> buffer, SQLLEN offset, SQLLEN length) {
        auto slice = buffer->Get(NanSymbol("slice"));
        assert (slice->IsFunction() && "The passed buffer object does not have a 'slice' function");

        auto sliceFn = Local<Function>::Cast(slice);
        Local<Value> argv[] = { NanNew<Integer>(offset), NanNew<Integer>(length) };
        auto result = sliceFn->Call(buffer, 2, argv);

        assert(result->IsObject() && "The result of slice() is not an object");
        return result->ToObject();
    }

    Persistent<Function> JSBuffer::constructor_;
    ClassInitializer<JSBuffer> jsBufferInit;
}

NODE_MODULE(eos, &Eos::Init)
