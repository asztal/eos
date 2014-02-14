#include "eos.hpp"
#include "uv.h"

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
        void InitError();
    }

    void Init(Handle<Object> exports) {
        EOS_DEBUG_METHOD();

        auto rec = &rootClassInitializer;
        while (rec = rec->next) {
            if (rec->Init)
                rec->Init(exports);
        }

        InitError();
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

        void ProcessCallbackQueue(uv_async_t*, int) {
            EOS_DEBUG_METHOD();

            uv_rwlock_rdlock(&rwlock);

            assert(firstCallback);

            while (firstCallback) {
                uv_rwlock_rdunlock(&rwlock);

                assert(firstCallback->target);
                
                auto fc = firstCallback; 
                auto ic = initCount;
                
                HANDLE hWait = firstCallback->target->GetWaitHandle();

                firstCallback->target->Notify();
                firstCallback->target->Unref();

                if (UnregisterWait(hWait))
                    DestroyWaiter();
                else 
                    EOS_DEBUG(L"UnregisterWait failed\n");

                // If initCount is zero, the last callback must have been called,
                // so firstCallback->next should be nullptr, and we don't need to
                // lock the lock since it will have been destroyed
                if (!initCount) {
                    assert(!firstCallback->next && "*Must* have been the last callback in the queue if initCount is 0");
                    return; 
                }

                uv_rwlock_rdlock(&rwlock);

                auto current = firstCallback;
                firstCallback = firstCallback->next;
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

            // Enqueue this callback before the others (easier)
            Callback* head = firstCallback;
            firstCallback = new Callback(reinterpret_cast<INotify*>(data));
            firstCallback->next = head;

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

    void Unwait(HANDLE& hRegisteredWaitHandle) {
        EOS_DEBUG_METHOD();
        if (UnregisterWait(hRegisteredWaitHandle)) {
            Async::DestroyWaiter();
            hRegisteredWaitHandle = nullptr;
        } else {
            EOS_DEBUG(L"UnregisterWait failed\n");
        }
    }

#pragma endregion

#pragma region("OdbcError")
    namespace {
        Persistent<Object> odbcErrorConstructor;
    
        void InitError() {
            EOS_DEBUG_METHOD();

            HandleScope scope;

            auto code = 
            "function OdbcError(message, state, subErrors) {\
                Error.apply(this, arguments);\
                this.message = message;\
                if (state) \
                    this.state = state;\
                if (subErrors)\
                    this.errors = subErrors;\
            }\
            OdbcError.prototype = new Error;\
            OdbcError.prototype.name = OdbcError.name;\
            OdbcError";

            odbcErrorConstructor = Persistent<Object>::New(Script::Compile(String::New(code))->Run()->ToObject());
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

        assert(handleType == SQL_HANDLE_ENV || handleType == SQL_HANDLE_DBC || handleType == SQL_HANDLE_STMT || handleType == SQL_HANDLE_STMT);
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

        if (nFields == 0)
            return scope.Close(Null());

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
                (SQLSMALLINT) sizeof(message),
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
}

NODE_MODULE(eos, &Eos::Init)
