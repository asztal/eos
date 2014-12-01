#pragma once

#include "eos.hpp"
#include "conn.hpp"
#include "handle.hpp"

namespace Eos {
    struct Parameter;

    struct Statement: EosHandle {
        static void Init(Handle<Object> exports);

        Statement(SQLHSTMT hStmt, Connection* connection EOS_ASYNC_ONLY_ARG(HANDLE hEvent));
        ~Statement();

#if defined(EOS_ENABLE_ASYNC_NOTIFICATIONS)
        void DisableAsynchronousNotifications();
#endif
        static const int HandleType = SQL_HANDLE_STMT;

    protected:
        void VirtualFree();

    public:

        static NAN_METHOD(New);
        NAN_METHOD(Prepare);
        NAN_METHOD(ExecDirect);
        NAN_METHOD(Execute);
        NAN_METHOD(Fetch);
        NAN_METHOD(GetData);
        NAN_METHOD(Cancel);
        NAN_METHOD(NumResultCols);
        NAN_METHOD(DescribeCol);

        NAN_METHOD(ParamData);
        NAN_METHOD(PutData);
        NAN_METHOD(MoreResults);
        
        NAN_METHOD(BindParameter);
        NAN_METHOD(SetParameterName);
        NAN_METHOD(UnbindParameters);

        NAN_METHOD(BindCol);
        NAN_METHOD(UnbindColumn);
        NAN_METHOD(UnbindColumns);

        NAN_METHOD(CloseCursor);

    public:

        // Non-JS methods
        static Handle<FunctionTemplate> Constructor() { return NanNew(constructor_); }

    protected:
        
        void AddBoundColumn(Parameter* col);
        void AddBoundParameter(Parameter* param);
        Parameter* GetBoundParameter(SQLUSMALLINT parameterNumber);

    private:
        Persistent<Array> boundParameters_;
        Persistent<Array> boundColumns_;

        Connection* connection_;

        static Persistent<FunctionTemplate> constructor_;
    };
}