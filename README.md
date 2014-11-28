What is this "eos"?
===================

 * It's an ODBC library for [Node.js](http://www.nodejs.org), with support for v0.10 and (tentatively) v0.11.
 * It makes **asynchronous** requests using the notification method on Windows 8.1, falling back to `libuv` on other platforms.
 * The aim is to be a set of **bindings** rather than a fully-fledged ODBC library, rather than providing unnecessary functionality which could be written in JavaScript. Hence it is small and fairly well documented.
 * Anything that can be asynchronous, is. It is possible to **stream large column values** by calling `SQLGetData` and `SQLPutData` in chunks.
 * It's **MIT-licensed**. Pull requests are welcome.
 
# Example

```js
var eos = require('eos');

function getCustomerName(customerID, callback) {
    var env = new eos.Environment();
    var conn = env.newConnection();

    conn.driverConnect("DSN=Customers;UID=Sara;PWD=12345", function(err) {
        if (err)
            return callback(err);

        var stmt = conn.newStatement();
        
        stmt.bindParameter(1, eos.SQL_PARAM_INPUT, eos.SQL_INTEGER, 0, 0, customerID);
        var name = stmt.bindCol(1, eos.SQL_WVARCHAR, new Buffer(100));
        
        stmt.execDirect("select name from Customers where ID = ?", function (err) {
            if (err)
                return callback(err);
            
            stmt.fetch(function(err, hasData) {
                if (!err)
                    return callback (err);
                if (!hasData)
                    return callback (new Error("Customer not found"));
                
                callback(null, name.value);
                
                // You'd actually want to handle exceptions here - see documentation of 
                // closeCursor for details.
                stmt.closeCursor();
                
                stmt.free();
                conn.disconnect(function(err) {
                    if (err)
                        console.log("Couldn't disconnect!", err.message);
                    
                    conn.free();
                    env.free();
                });
            });
        });
    });
}
```

# API

There are 4 types of handles in ODBC: environment, connection, statement, and descriptor. Eos
wraps these handles in JavaScript objects. The handle will automatically be freed when the 
JavaScript object is garbage collected, but you may choose to do so earlier.

Eos makes no attempt to provide bells and whistles: this is a low-level wrapper around ODBC which
can be used as a building block for a more high-level library.

Most functions in Eos are asynchronous (any ODBC call which can return ``SQL_STILL_EXECUTING``), however some are guaranteed to return synchronously and are
marked here with _(synchronous)_ accordingly.

### Asynchronous execution

ODBC itself provides for synchronous calls, and asynchronous calls using either [polling](http://msdn.microsoft.com/en-us/library/ms713563%28v=vs.85%29.aspx) (requires ODBC 3.80, e.g. Windows 7) or the [notification method](http://msdn.microsoft.com/en-us/library/hh405038%28v=vs.85%29.aspx) (requires ODBC 3.81, e.g. Windows 8). 

Eos uses the notification method where possible, and falls back to using synchronous calls on the  [libuv](https://github.com/joyent/libuv) thread pool where the notification method is not supported. The main advantage of the notification method is that fewer thread pool threads are used (1 thread per 64 concurrent operations, rather than 1 thread for each operation). The polling asynchronous method is not supported, but could be implemented. Synchronous versions of asychronous API calls are not yet implemented, but could also be done.

### Documentation syntax

 * Most methods are asynchronous, the few that are synchronous are marked _(synchronous)_. Synchronous calls that raise errors will throw the error as a JavaScript exception.
 * Optional parameters are enclosed in square brackets, e.g.:
   Environment.dataSources([type])
 * Callback parameters are declared as: Connection.connect(connectionString, callback([err], arg1, [arg2]))
   A parenthesised argument list follows the callback name, with square brackets for optional arguments as usual. All callbacks in Eos take an _error_ parameter as the first argument, which will be undefined when there is no error. Many callbacks take only an _error_ parameter, in which case the argument list here is omitted and the _error_ parameter is implied.
 * ODBC API calls are referred to in bold, e.g. **SQLExecDirect**.

### Error Handling

Errors returned from the ODBC driver manager or the ODBC driver will be an instance of the `OdbcError` constructor. Asynchronous calls may still throw errors synchronously, in cases such as invalid arguments.

ODBC errors look something like this:

```js
{ message: '[Microsoft][ODBC Driver Manager] Data source name not found and no default driver specified',
  state: 'IM002',
  errors:
   [ { message: '[Microsoft][ODBC Driver Manager] Data source name not found and no default driver specified',
       state: 'IM002' },
     { message: '[Microsoft][ODBC Driver Manager] Invalid connection string attribute',
       state: '01S00' } ] }
```

ODBC can return multiple errors for a single operation. In such cases, the first error is the main error returned, however the full list of errors is returned in the `errors` property.

## Data types

The mapping between SQL data types and JavaScript data types is fairly simple. This table will help
you understand what SQL type to pass to `getData` to get back the correct data type, and what to pass to
`bindParameter` to send/receive parameters.

 * The _SQL data type_ column refers to the type you pass to `getData` or `bindParameter`.
 * The _C type_ column is for informational purposes and specifies how the data is stored in the
parameter/column's `buffer` and `bytesInBuffer` properties. The format of the buffer will be 
driver and architecture-dependent, however.
 * The _SQL→JS_ column refers to the default conversion when returning results from
`getData` (which you can work around by accessing the parameter's buffer directly) or receiving 
the values of output parameters.
 * The _JS→SQL_ column specifies how JavaScript values are converted for storing into the
parameter buffer _(see `FillInputBuffer` in buffer.cpp)_.

SQL data type | C type | SQL→JS | JS→SQL
--------------|--------|----------------------|---------------
`SQL_INTEGER`, `SQL_SMALLINT`, `SQL_TINYINT` | `SQL_C_SLONG` | `Number` | Coerced to Int32
`SQL_NUMERIC`, `SQL_DECIMAL`, `SQL_BIGINT`, `SQL_FLOAT`, `SQL_REAL`, `SQL_DOUBLE` | `SQL_C_DOUBLE` | `Number` | Coerced to Number
`SQL_DATETIME`, `SQL_TIMESTAMP` | `SQL_C_TYPE_TIMESTAMP` | `Date` | Fails if the value is not a date (TODO: specify: in what way does it fail and how is null handled?)
`SQL_BIT` | `SQL_C_BIT` | `true`/`false` | Coerced to boolean
`SQL_BINARY`, `SQL_VARBINARY`, `SQL_LONGVARBINARY`¹ | `SQL_C_BINARY` | `Buffer` | None necessary
`SQL_CHAR`, `SQL_VARCHAR`, `SQL_LONGVARCHAR`¹ | `SQL_C_WCHAR` | `String` | Coerced to a string and encoded in UTF-8
`SQL_WCHAR`, `SQL_WVARCHAR`, `SQL_WLONGVARCHAR`¹, everything else | `SQL_C_WCHAR` | `String` | Coerced to a string and encoded in UTF-16

¹ MSSQL has limitations binding `SQL_[W]LONGVARCHAR` and `SQL_LONGVARBINARY` columns, 
especially with data-at-execution parameters. It is advised to use `SQL_[W]VARCHAR` and 
`SQL_VARBINARY` instead, with a `columnSize` of zero, because the long data types map
to the deprecated `[n]text` and `image` data types. (Note: FreeTDS does not support zero
column sizes for `SQL_[W]VARCHAR` and `SQL_VARBINARY`, unfortunately.)

## Environment

An `Environment` is a wrapper around a `SQLHENV` which is used to enumerate drivers and data 
sources, and to allocate new connections. There may be many environments allocated at any one
time. 

The **SQLEndTran** operation is deliberately unimplemented, as it does not work when any connection has asynchronous execution enabled.

### new Environment()

Wraps **SQLAllocHandle**. Creates a new ODBC environment handle and wraps it in a JavaScript object.

### Environment.newConnection() _(synchronous)_

Wraps **SQLAllocHandle**. Creates a new `Connection` in the current environment. The new connection will initially be disconnected.

### Environment.dataSources([type]) _(synchronous)_

Wrap **SQLDataSources**. Enumerates available data sources. `type` can be any of the following:

 * `"user"` lists all user DSNs.
 * `"system"` lists all system DSNs.
 * _(omitted)_: lists all user and system DSNs.

The data sources are returned as { server, description } pairs, e.g.
```js
[ { server: 'Products',
    description: 'Microsoft ODBC for Oracle' },
  { server: 'Customers',
    description: 'ODBC Driver 11 for SQL Server' } ]
```

The `server` property may be used as a DSN for `Connection.browseConnect`.

### Environment.drivers()

Wraps **SQLDrivers**. Enumerates available ODBC drivers. These could be used to build a connection
string using **SQLBrowseConnect**. Example output:

```js
[ { description: 'Microsoft Access Text Driver (*.txt, *.csv)',
    attributes:
     [ 'UsageCount=3',
       'APILevel=1',
       'ConnectFunctions=YYN',
       'DriverODBCVer=02.50',
       'FileUsage=2',
       'FileExtns=*.txt, *.csv',
       'SQLLevel=0',
       'CPTimeout=<not pooled>' ] },
  { description: 'SQL Server Native Client 11.0',
    attributes:
     [ 'UsageCount=1',
       'APILevel=2',
       'ConnectFunctions=YYY',
       'CPTimeout=60',
       'DriverODBCVer=03.80',
       'FileUsage=0',
       'SQLLevel=1' ] },
  { description: 'SQL Server Native Client 10.0',
    attributes:
     [ 'UsageCount=1',
       'APILevel=2',
       'ConnectFunctions=YYY',
       'CPTimeout=60',
       'DriverODBCVer=10.00',
       'FileUsage=0',
       'SQLLevel=1' ] },
  { description: 'ODBC Driver 11 for SQL Server',
    attributes:
     [ 'UsageCount=1',
       'APILevel=2',
       'ConnectFunctions=YYY',
       'CPTimeout=60',
       'DriverODBCVer=03.80',
       'FileUsage=0',
       'SQLLevel=1' ] } ]
```

The following information about data source names can be found on the MSDN page about [Driver Specification Subkeys](http://msdn.microsoft.com/en-us/library/ms714538%28v=vs.85%29.aspx).

#### DriverODBCVer

The ODBC version of the driver. The ODBC version of the application and the driver need not match exactly &mdash; the Driver Manager will translate as necessary. See the [Compatibility Matrix](http://msdn.microsoft.com/en-us/library/ms709305%28v=vs.85%29.aspx) for more details.

#### ConnectFunctions

A string of three Ys or Ns. If not specified, NNN should be assumed.

 * The first Y or N declares support for **SQLConnect**.
 * The second Y or N declare support for **SQLDriverConnect**.
 * The third Y or N declares support for **SQLBrowseConnect**.
 
E.g. the Microsoft Access Text Driver defined above does not support **SQLBrowseConnect**.

#### FileUsage

FileUsage | Means
--- | ---
0 | Not file based
1 | Files are treated as tables
2 | Files are treated as databases

#### SQLLevel 

My limited research has been able to determine that the following values for _SQLLevel_ are possible:

SQLLevel | Means
--- | --- 
0 | Basic SQL-92 Compliance
1 | FIPS127-2 Transitional
2 | SQL-92 Intermediate
3 | SQL-92 Full

### Environment.free() _(synchronous)_

Destroys the environment handle.

## Connection

A `Connection` is a wrapper around a `SQLHDBC` which is used to connect to data sources and create
statements to execute. A connection may be in one of five states once allocated:

State| Means
--- | ---
C2 | Allocated
C3 | Connecting via `browseConnect` (need more data)
C4 | Connected
C5 | Connected, allocated statement
C6 | Connected, in transaction

### Connection.connect(connectionString, callback)

Wraps **SQLDriverConnect**. Takes a complete connection string and connects to it. If any required
connection string parameters are missing, the operation will fail.

### Connection.browseConnect(inConnectionString, callback([err], more, outConnectionString))

Wraps **SQLBrowseConnect**. Iteratively connects to a data source, requesting more and more 
information until the connection string is complete. The ODBC driver requests more information
by returning a new connection string, _outConnectionString_ (also referred to as the _browse 
result connection string_) which contains information about which parameters (_keywords_) can
be supplied, which are required and optional, possible values (if there is a restricted choice),
and user-friendly keyword labels.

_inConnectionString_ should initially be a connection string with only the *DRIVER* or *DSN* 
keyword set.

When the callback is called with _more_ is set to `true`, more data is required to complete 
the connection. There two cases: either further information is needed, or the data filled in
in the last call was invalid (e.g. an incorrect password). The browse result connection string
should be parsed, and the required missing values filled in. Once the values are filled in,
`browseConnect` should be called again with the new connection string as the _inConnectionString_
parameter.

When the callback is called with _more_ set to `false`, the connection is connected. The
connection string _outConnectionString_ is complete and can be used as a parameter to a future
call to `connect()`, bypassing the `browseConnect` process.

If an error (_err_) occurs, the connection is reset to the disconnected state. 

To cancel a `browseConnect` operation, call `disconnect()`.

This function is not supported when connection pooling is enabled.

### Connection.newStatement() _(synchronous)_

Creates a new `Statement` object, which can be used for preparing statements and executing SQL directly.

### Connection.disconnect(callback)

Disconnects from the data source. After a successful disconnect operation, the connection handle may be used again to connect to another data source.

### Connection.free() _(synchronous)_

Destroys the connection handle.

## Statement 

A `Statement` is a wrapper around a `SQLHSTMT` and can be obtained via `conn.newStatement()`. Statements represent SQL statements which can be prepared and executed with bound parameters, and can return any number of record sets (including none).

### Statement.prepare(sql, callback)

Wraps **SQLPrepare**. Prepare the statement using given SQL, which may contain wildcards to be replaced by [bound parameters](http://msdn.microsoft.com/en-us/library/ms712522%28v=vs.85%29.aspx). If successful, the prepared statement can be executed using `Statement.execute()`.

### Statement.execute(callback [err, needData, dataAvailable]) 

Executes the prepared statement. 
If there are data-at-execution parameters whose values have not yet been specified, the callback will be called 
with _needData_ set to true. In this case, call `Statement.paramData()` to determine which input parameter is
required (the order which data-at-execution parameters are requested is defined by the driver).

After successful execution, the cursor will be positioned before the first result set. 
To start reading the first result set (if there is any), call `Statement.fetch()`.

If there are streamed output or input/output parameters, _hasData_ may be true (provided that there are no 
warning messages, result sets, or input parameters. If so, those must be dealt with first). In this case, call
`Statement.paramData()` to determine which output parameter has data available (as with input parameters, the order 
in which output parameters become available is defined by the driver). If there are result sets or warning messages,
use `Statement.moreResults()` to retrieve streamed output parameters.

### Statement.execDirect(sql, callback [err, needData, dataAvailable])

Wraps **SQLExecDirect**, used to execute SQL without preparing. The same as `Statement.execute()`, except there is no need to call `Statement.prepare()`. Generally, this is preferable to using `prepare` then `execute`, [unless the same statement is likely to be executed more than 3 to 5 times](http://msdn.microsoft.com/en-us/library/ms811006.aspx#code-snippet-35), in which case preparing the statement may perform better. `execDirect` accepts bound parameters.

If _dataAvailable_ is true, there are output parameters whose value is now available to read using `Statement.getData()`.

### Statement.fetch(callback [err, hasData])

Wraps **SQLFetch**, used to fetch the next row of a result set. If successful, _hasData_ indicates whether or not the cursor is positioned on a result set.

### Statement.moreResults(callback [err, hasData, hasParamData])

Wraps **SQLMoreResults**, used to move to the next result set. If _hasData_ is true, the cursor is positioned on a result set, and `Statement.fetch()` can be used. If _hasParamData_ is true, the last result set has been read and there are output parameters available to read using `Statement.getData()`.

### Statement.paramData(callback [err, param, needData, dataAvailable])

Wraps **SQLParamData**, used in two circumstances.
 * When `Statement.execute()` or `Statement.execDirect()` returns _needData_ as true, the driver is requesting data for a bound parameter of type `SQL_PARAM_INPUT_STREAM` or `SQL_PARAM_INPUT_OUTPUT_STREAM`. _param_ is a `Parameter` object representing the parameter requested, and can be passed to `Statement.putData` to send the parameter in chunks. Parameters are generally not guaranteed to be requested sequentially.
 * When `Statement.execute()`, `Statement.execDirect()`, or `Statement.moreResults()` returns _dataAvailable_ as true, _param_ will be a `Parameter` representing the parameter whose data is available to read using `Statement.getData()`. Parameters are generally not guaranteed to become available sequentially.

### Statement.numResultCols(callback [err, count]) 

Wraps **SQLNumResultCols**. Calls the callback with _count_ as the number of result columns. Only valid if the cursor is positioned on a result set.

### Statement.describeCol(columnNumber, callback [err, name, dataType, columnSize, decimalDigits, nullable)

Wraps **SQLDescribeCol**. Returns information about the column at index `columnNumber`, where 1 is the first column and 0 is the bookmark column (if bookmark columns are enabled). 
 * _name_ is the name of the column.
 * _dataType_ is a numeric value representing the SQL data type of the column (e.g. `SQL_VARCHAR`, `SQL_INTEGER`, `SQL_BINARY`)
 * _columnSize_ is an integer representing the number of bytes required to store the column value. _(Note: This may not be accurate, e.g. SQL Server may return 0 for `varchar(max)` columns.)_
 * _decimalDigits_ is the number of digits after the decimal point supported by the column data type, for integral values.
 * _nullable_ is either _true_ (if the column value may be null), _false_ (if the column value cannot be null), or _undefined_ (if the nullability of the column is unknown).
 
### Statement.getData(columnNumber, dataType, [buffer], raw, callback [err, result, totalBytes, more])

Wraps **SQLGetData**. Retrieves the value of a column, coerced to the value specified by 
_dataType_ (e.g. `SQL_INTEGER`). Most SQL data types can be represented as JavaScript values.
If the column has a long value, only a portion of the value will be fetched. 
The size of this portion depends on the size of the _buffer_ passed in (if no buffer is 
passed in, a 64KiB buffer is created).
To aid in figuring out whether more data exists to retrieve, the _more_ callback parameter
is true when there is more data to get (or when the entire length of the column is unknown,
in which case it is assumed that there is more data to retrieve).

Successive `getData` calls will retrieve successive chunks of the column value, 
until _result_ is `undefined` (if `getData` is called after the callback is called with `more`
set to _false_).

If _dataType_ is `SQL_BINARY`, the results (or a portion of) will be placed into _buffer_.
_buffer_ may be a `Buffer` or a `SlowBuffer`.
If none is passed, a new 64KiB `Buffer` is allocated. 
If the call to `getData` does not use the entire buffer, a slice of the input buffer is returned,
otherwise the buffer itself is returned.

If _dataType_ is a character type, the results will also be placed into _buffer_ (creating a 64KiB
`Buffer` if none is given), however the results will be converted to a `String` 
(unless _raw_ is true, see below). 
`SQL_WCHAR` and such will be treated as UTF-16, and normal character data will be treated as UTF-8.
If using raw mode, be aware that ODBC always writes a null terminator after character 
data in a buffer. 
If `totalBytes` is less than the length of the buffer passed in, the first _totalLength_ bytes 
are all character data (i.e. `buf.toString(encoding, 0, totalBytes)` is valid). 
If `totalBytes` is greater than or equal to the length of the buffer, or undefined,
you should subtract the length of the null terminator (1 byte for SQL_CHAR, 2 bytes for SQL_WCHAR)
from the number of bytes (or from the length of the buffer, if `totalBytes` is undefined). 

If _raw_ is true, _result_ will simply be the _buffer_ which was passed in
(or an automatically-allocated buffer, if none was given). The buffer will 
not be sliced; it is up to the caller to use _totalBytes_ to determine what 
to do and to read the data in the correct format. (Note: _totalBytes_ may be 
`undefined` if the total length of the value is unknown. In this case the buffer will be full.)

### Statement.bindParameter(index, kind, type, columnSize, decimalDigits, [value], [buffer]) _(synchronous)_

Wraps **SQLBindParameter**. This function binds a parameter and returns a `Parameter` object.

 * `index` specifies which parameter number to bind, starting from 1.
 * `kind` specifies whether the parameter is an input parameter, output parameter, or both. In fact
 it is more complex than this, which will be explained below.
 * `type` refers to the SQL type of the parameter (e.g. `SQL_VARCHAR`, `SQL_INTEGER`, `SQL_VARBINARY`)
 * `columnSize` depends on the type of parameter. For variable length types (string, buffer) it refers
 to the length of the data. For non-integral numbers, it refers to the precision. 
 * `decimalDigits` usually refers to the number of decimal digits for fractional seconds in date/time
 data types, however it can also refer to the _scale_ of `SQL_NUMERIC` or `SQL_DECIMAL` data types.
 * `value`, if passed, binds the parameter with the specified value. If `null` is passed, the parameter's
 value is null, but if `undefined` is passed, it will mark the parameter as a Data At Execution parameter.
 * `buffer`, if passed, will specify the `Buffer` used to store the parameter's data. If none is passed, 
 a buffer will automatically be allocated when the value is set (either by passed to `bindParameter`, or
 later calling `Parameter.set`.

#### Kinds of parameters

Parameters can be input parameters, output parameters, or both. Output parameters can be either streamed 
(the value is retrieved in parts using `getData`) or bound (the value is automatically set when the statement
is executed), and similarly input parameters can be streamed (sent in parts with `putData`, known as Data At 
Execution) or bound (sent as part of `execute` or `execDirect`). To specify a Data At Execution parameter, 
pass `undefined` for the `value` argument of `bindParameter`.

|`kind`|value|Input|Output|
|------|-----|-----|------|
|`SQL_PARAM_INPUT`|`undefined`|DAE||
|`SQL_PARAM_INPUT`|a value|Bound||
|`SQL_PARAM_INPUT_OUTPUT`|`undefined`|DAE|Bound|
|`SQL_PARAM_INPUT_OUTPUT`|a value|Bound|Bound|
|`SQL_PARAM_INPUT_OUTPUT_STREAM`|`undefined`|DAE|Streamed|
|`SQL_PARAM_INPUT_OUTPUT_STREAM`|a value|Bound|Streamed|
|`SQL_PARAM_OUTPUT`|||Bound|
|`SQL_PARAM_OUTPUT_STREAM`|||Streamed|

### Statement.putData(parameter, [buffer], [bytes], callback [err, needData, dataAvailable])

Wraps **SQLPutData**. Used for sending parameter values in chunks (known as _data at execution_). 
`parameter` should the `Parameter` object returned by `paramData` when another ODBC call returns
`needData` as true.

When writing binary columns, simply fill `parameter.buffer` with the data to send before calling
`putData` (e.g. by calling `fs.read` and passing it the buffer).

If `buffer` is passed, it will be used instead of the `parameter.buffer`. If `bytes` is a number, it 
specifies how many bytes of data are in the buffer; if `bytes` is null then `null` will be sent (this
is only valid for the first chunk); if `bytes` is anything else, buffer will be assumed to be full.

*TODO*: Figure out the precise semantics when **SQLPutData** returns `SQL_NEED_DATA` and when it doesn't,
and if it's OK to send more data even if the server doesn't ask for it (I think it's OK).

### Statement.closeCursor([throwOnNoCursor = false]) _(synchronous)_

Wraps **SQLCloseCursor**. Closes the current cursor associated with a statement (i.e. makes the statement
ready for another `execute` call). If `throwOnNoCursor` is true (which it isn't by default), the call will
throw an exception if there is not currently an active result set.

Even if `throwOnNoCursor` is not set, `closeCursor` can still throw an exception, so be careful. An examples
of this are the `08S01` SQLSTATE (Communication link failure). If this error is thrown, the only option is
to free the `Statement` and disconnect the `Connection`.

### Statement.free() _(synchronous)_

Destroys the statement handle.
