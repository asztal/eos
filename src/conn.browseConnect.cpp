#include "conn.hpp"

using namespace Eos;

namespace Eos {
    struct BrowseConnectOperation : Operation<Connection, BrowseConnectOperation> {
        BrowseConnectOperation::BrowseConnectOperation(Handle<Value> connectionString)
            : connectionString_(connectionString)
        {
            EOS_DEBUG_METHOD();
            EOS_DEBUG(L"Connection string: %ls\n", *connectionString_);
        }

        static Handle<Value> New(Connection* owner, const Arguments& args) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 3)
                return ThrowError("Too few arguments");

            if (!args[1]->IsString())
                return ThrowTypeError("Connection string should be a string");

            (new BrowseConnectOperation(args[1]))->Wrap(args.Holder());
            return args.Holder();
        }

        static const char* Name() { return "BrowseConnectOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            return SQLBrowseConnectW(
                Owner()->GetHandle(), 
                *connectionString_, connectionString_.length(),
                outConnectionString_, outConnectionStringBufferLength, &outConnectionStringLength_);
        }

        void CallbackOverride(SQLRETURN ret) {
            EOS_DEBUG_METHOD();

            if (!SQL_SUCCEEDED(ret) && ret != SQL_NEED_DATA)
                return CallbackErrorOverride(ret);

            EOS_DEBUG(L"Final Result: %hi\n", ret);

            Handle<Value> argv[] = { 
                Undefined(),
                ret == SQL_NEED_DATA ? True() : False(),
                StringFromTChar(outConnectionString_, min(outConnectionStringLength_, (SQLSMALLINT)outConnectionStringBufferLength)) 
            };

            EOS_DEBUG(L"Result: %i, %ls\n", ret, outConnectionString_);
            
            GetCallback()->Call(Context::GetCurrent()->Global(), 3, argv);
        }

    protected:
        WStringValue connectionString_;
        enum { outConnectionStringBufferLength = 4096 };
        wchar_t outConnectionString_[outConnectionStringBufferLength + 1];
        SQLSMALLINT outConnectionStringLength_;
    };
}

Handle<Value> Connection::BrowseConnect(const Arguments& args) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 2)
        return ThrowError("Connection::BrowseConnect() requires 2 arguments");

    Handle<Value> argv[] = { handle_, args[0], args[1] };
    return Begin<BrowseConnectOperation>(argv);
}

Persistent<FunctionTemplate> Operation<Connection, BrowseConnectOperation>::constructor_;
namespace { ClassInitializer<BrowseConnectOperation> ci; }