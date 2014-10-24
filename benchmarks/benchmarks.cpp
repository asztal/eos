#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <cstdio>
#include <ctime>

int main(int argc, char* argv[]) {
    SQLHENV hEnv = SQL_NULL_HANDLE;
    SQLHDBC hDbc = SQL_NULL_HANDLE;
    SQLHSTMT hStmt = SQL_NULL_HANDLE;

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv)))
        goto error;

    if (!SQL_SUCCEEDED(SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3_80, NULL)))
        goto error;

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc)))
        goto error;

    if (!SQL_SUCCEEDED(SQLDriverConnect(hDbc, SQL_NULL_HANDLE, L"Driver=ODBC Driver 11 for SQL Server;Server=localhost,3800;Trusted_Connection=Yes", SQL_NTS, NULL, 0, NULL, SQL_DRIVER_NOPROMPT)))
        goto error;

    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt)))
        goto error;

    if (!SQL_SUCCEEDED(SQLPrepare(hStmt, L"select ? + ?, ?", SQL_NTS)))
        goto error;

    SQLINTEGER x, y, z;
    SQLWCHAR str[] = L"This is a string", res[30];
    SQLLEN cbRes;

    if (!SQL_SUCCEEDED(SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &x, sizeof(x), NULL)))
        goto error;
    
    if (!SQL_SUCCEEDED(SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &y, sizeof(y), NULL)))
        goto error;
    
    if (!SQL_SUCCEEDED(SQLBindParameter(hStmt, 3, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR, 0, 0, str, sizeof(str), NULL)))
        goto error;

    if (!SQL_SUCCEEDED(SQLBindCol(hStmt, 1, SQL_C_SLONG, &z, sizeof(z), NULL)))
        goto error;

    if (!SQL_SUCCEEDED(SQLBindCol(hStmt, 2, SQL_C_WCHAR, res, sizeof(res), &cbRes)))
        goto error;

    const int N = 10000;

    auto start = GetTickCount64();

    for (int i = 0; i < N; i++) {
        x = rand() % 1000;
        y = rand() % 1000;

        if (!SQL_SUCCEEDED(SQLExecute(hStmt)))
            goto error;

        if (!SQL_SUCCEEDED(SQLFetch(hStmt)))
            goto error;

        // printf("%d, %d, %d, %ls\n", x, y, z, res);

        if (!SQL_SUCCEEDED(SQLCloseCursor(hStmt)))
            goto error;
    }

    auto end = GetTickCount64();
    printf("Completed %d runs in %dms (%d/sec)\n", N, int(end - start), int(N * 1000 / (end - start)));

    system("pause");
	return 0;

error:
	SQLWCHAR state[5 + 1] = {0}, message[1024 + 1] = {0};

    for (int j = 0; j < 3; j++) {
        SQLRETURN ret = SQL_SUCCESS;
        SQLSMALLINT type;
        SQLHANDLE handle;

        switch (j) {
        case 0: type = SQL_HANDLE_ENV; handle = hEnv; break;
        case 1: type = SQL_HANDLE_DBC; handle = hDbc; break;
        case 2: type = SQL_HANDLE_STMT; handle = hStmt; break;
        }

        for (int i = 0; handle && SQL_SUCCEEDED(ret); i++) {
            ret = SQLGetDiagRecW(type, handle, i + 1, state, NULL, message, sizeof(message), NULL);
            if (SQL_SUCCEEDED(ret))
                printf("%ls: %ls\n", state, message);
        }
    }

    system("pause");
    return 1;
}

