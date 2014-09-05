#include "conn.hpp"

using namespace Eos;

namespace Eos {
    struct ConnectOperation : Operation<Connection, ConnectOperation> {
        ConnectOperation(
            SQLWCHAR* serverName, SQLSMALLINT serverNameLength,
            SQLWCHAR* userName, SQLSMALLINT userNameLength,
            SQLWCHAR* password, SQLSMALLINT passwordLength)
            : serverName_(serverName), serverNameLength_(serverNameLength)
            , userName_(userName), userNameLength_(userNameLength)
            , password_(password), passwordLength_(passwordLength)
        {
#if defined(DEBUG)
	    std::wstring 
		sServerName = sqlstring(serverName),
		sUserName = sqlstring(userName),
		sPassword = sqlstring(password);
	    EOS_DEBUG_METHOD_FMT(L"%ls, %ls, %ls", 
				 serverName ? sServerName.c_str() : L"<nullptr>", 
				 userName ? sUserName.c_str() : L"<nullptr>", 
				 password ? sPassword.c_str() : L"<nullptr>");
#endif
        }

        ~ConnectOperation() {
            EOS_DEBUG_METHOD();

            delete[] serverName_;
            delete[] userName_;
            delete[] password_;
        }

        static EOS_OPERATION_CONSTRUCTOR(New, Connection) {
            EOS_DEBUG_METHOD();

            if (args.Length() < 2)
                return NanError("Too few arguments");

            SQLWCHAR* str[3] = { nullptr, nullptr, nullptr };
            SQLSMALLINT len[3] = { 0, 0, 0 };

            for (int i = 0; i < 3; i++) {
               if (i + 1 >= args.Length() - 1
                   || args[i+1]->IsNull()
                   || args[i+1]->IsUndefined())
                   continue;

               if (args[i+1]->IsString())
                   len[i] = 1;
               else 
                   return NanTypeError("Expected a string");
            }

            bool oom = false;
            for (int i = 0; i < 3; i++) {
                if (!len[i])
                    continue;

                WStringValue sv(args[i+1]);
                len[i] = sv.length();
                str[i] = new (nothrow) SQLWCHAR[len[i]];
                if (str[i])
		    sqlwcsncpy(str[i], *sv, len[i]);
                else
                    oom = true;
            }

            if (oom) {
                delete[] str[0];
                delete[] str[1];
                delete[] str[2];
                return NanError("Out of memory");
            }

            (new ConnectOperation(
                str[0], len[0], 
                str[1], len[1], 
                str[2], len[2]))->Wrap(args.Holder());

            EOS_OPERATION_CONSTRUCTOR_RETURN();
        }

        static const char* Name() { return "ConnectOperation"; }

    protected:
        SQLRETURN CallOverride() {
            EOS_DEBUG_METHOD();

            return SQLConnectW(
                Owner()->GetHandle(),
                serverName_, serverNameLength_,
                userName_, userNameLength_,
                password_, passwordLength_);
        }

        // TODO When do these get deleted?

    protected:
        SQLWCHAR *serverName_, *userName_, *password_;
        SQLSMALLINT serverNameLength_, userNameLength_, passwordLength_;
    };
}

NAN_METHOD(Connection::Connect) {
    EOS_DEBUG_METHOD();

    if (args.Length() < 2)
        return NanThrowError("Connection::Connect() requires 2 arguments");

    if (args.Length() == 2) {
        Handle<Value> argv[] = { NanObjectWrapHandle(this), args[0], args[1] };
        return Begin<ConnectOperation>(argv);
    } else if (args.Length() == 3) {
        Handle<Value> argv[] = { NanObjectWrapHandle(this), args[0], args[1], args[2] };
        return Begin<ConnectOperation>(argv);
    } else {
        Handle<Value> argv[] = { NanObjectWrapHandle(this), args[0], args[1], args[2], args[3] };
        return Begin<ConnectOperation>(argv);
    }
}

template<> Persistent<FunctionTemplate> Operation<Connection, ConnectOperation>::constructor_ = Persistent<FunctionTemplate>();
namespace { ClassInitializer<ConnectOperation> ci; }
