#include "eos.hpp"
#include "conn.hpp"

namespace Eos {
    struct Statement: ObjectWrap {
        static void Init(Handle<Object> exports);

        Statement(SQLHSTMT hStmt, Connection* connection);
        ~Statement();

    public:

        static Handle<Value> New(const Arguments& args);
        Handle<Value> Free(const Arguments& args);

    public:

        // Non-JS methods
        SQLHENV GetHandle() const { return hStmt_.Value(); }
        Local<Value> GetLastError() { return Eos::GetLastError(hStmt_); }
        static Persistent<FunctionTemplate> Constructor() { return constructor_; }

    private:
        Statement(const Statement&); // = delete;
        
        Connection* connection_;
        ODBCHandle<SQL_HANDLE_STMT> hStmt_;

        static Persistent<FunctionTemplate> constructor_;
    };
}