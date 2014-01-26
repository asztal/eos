#pragma once 

#include "eos.hpp"
#include "env.hpp"

namespace Eos {
    struct Connection : ObjectWrap, INotify {
        static void Init(Handle<Object> exports);

        Connection(Environment* environment, SQLHDBC hDbc, HANDLE hEvent); 
        ~Connection();

        static const int HandleType = SQL_HANDLE_DBC;

    public:
        // JS methods
        static Handle<Value> New(const Arguments& args);
        Handle<Value> Connect(const Arguments& args);
        Handle<Value> NewStatement(const Arguments& args);
        Handle<Value> Free(const Arguments& args);

    public:
        // Non-JS methods
        SQLHDBC GetHandle() const { return hDbc_.Value(); }
        HANDLE GetWaitHandle() const { return hEvent_; }
        Local<Value> GetLastError() { return Eos::GetLastError(hDbc_); }
        static Persistent<FunctionTemplate> Constructor() { return constructor_; }
        IOperation* Operation() { return ObjectWrap::Unwrap<IOperation>(operation_); }
        void Notify() {
            EOS_DEBUG_METHOD();
            // Do something, maybe
            
            Operation()->OnCompleted();
        }

    private:
        Connection(const Connection&); // = delete;

        Environment* environment_;
        HANDLE hEvent_, hWait_;
        Persistent<Object> operation_;
        ODBCHandle<SQL_HANDLE_DBC> hDbc_;

        static Persistent<FunctionTemplate> constructor_;
    };
}
