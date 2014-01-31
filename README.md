eos
===

 * It's an ODBC library for [Node.js](http://www.nodejs.org).
 * It makes asynchronous requests (using the notification method) where possible.
 * The aim is to be minimal and well-documented.
 * It's MIT-licensed.
 
# Example

```js

var eos = require('eos');

var env = new eos.Environment();
var conn = env.newConnection();

conn.connect("DSN=Customers;UID=Sara;PWD=12345", function(err) {
	if (err)
    	return console.log("Couldn't connect!", err.message);

	var stmt = conn.newStatement();
    
    // At this point, I guess we'd do something with the statement.
    // It would probably help if I'd implemented any statement operations.
    console.log("Just making a statement.");
    
    stmt.free();
    
    conn.disconnect(function(err) {
    	if (err)
        	console.log("Couldn't disconnect!", err.message);
    
    	conn.free();
        env.free();
    });
});



```

# API

There are 4 types of handles in ODBC: environment, connection, statement, and descriptor. Eos
wraps these handles in JavaScript objects. The handle will automatically be freed when the 
JavaScript object is garbage collected, but you may choose to do so earlier.

Eos makes no attempt to provide bells and whistles: this is a low-level wrapper around ODBC which
can be used as a building block for a more high-level library.

Most functions in Eos are asynchronous, however some are guaranteed to return synchronously and are
marked here with _(synchronous)_ accordingly.

### Asynchronous execution

ODBC provides for synchronous calls, and asynchronous calls using either [polling](http://msdn.microsoft.com/en-us/library/ms713563%28v=vs.85%29.aspx) (requires ODBC 3.80, e.g. Windows 7) or the [notification method](http://msdn.microsoft.com/en-us/library/hh405038%28v=vs.85%29.aspx) (requires ODBC 3.81, e.g. Windows 8). 

Currently Eos uses only the notification method, but will be extended to support the polling method and the eventually using the [libuv](https://github.com/joyent/libuv) thread pool where asynchronous execution is not supported at all.

### Documentation syntax

 * Most methods are asynchronous, the few that are synchronous are marked _(synchronous)_. Synchronous calls that raise errors will throw the error as a JavaScript exception.
 * Optional parameters are enclosed in square brackets, e.g.:
   Environment.dataSources([type])
 * Callback parameters are declared as: Connection.connect(connectionString, callback([err], arg1, [arg2]))
   A parenthesised argument list follows the callback name, with square brackets for optional arguments as usual. All callbacks in Eos take an _error_ parameter as the first argument, which will be undefined when there is no error. Many callbacks take only an _error_ parameter, in which case the argument list here is omitted and the _error_ parameter is implied.
 * ODBC API calls are referred to with bold code tags, e.g. **`SQLExecDirect`**.

### Error Handling

Errors returned from the ODBC driver manager or the ODBC driver will be an instance of the `OdbcError` constructor. Asynchronous calls may still throw errors synchronously, in cases such as invalid arguments.

ODBC errors look something like this:

```json
{ message: '[Microsoft][ODBC Driver Manager] Data source name not found and no default driver specified',
  state: 'IM002',
  errors:
   [ { message: '[Microsoft][ODBC Driver Manager] Data source name not found and no default driver specified',
       state: 'IM002' },
     { message: '[Microsoft][ODBC Driver Manager] Invalid connection string attribute',
       state: '01S00' } ] }
```

ODBC can return multiple errors for a single operation. In such cases, the first error is the main error returned, however the full list of errors is returned in the `errors` property.

## Environment

An `Environment` is a wrapper around a `SQLHENV` which is used to enumerate drivers and data 
sources, and to allocate new connections. There may be many environments allocated at any one
time. 

The **`SQLEndTran`** operation is deliberately unimplemented, as it does not work when any connection has asynchronous execution enabled.

### new Environment()

Wraps **`SQLAllocHandle`**. Creates a new ODBC environment handle and wraps it in a JavaScript object.

### Environment.newConnection() _(synchronous)_

Wraps **`SQLAllocHandle`**. Creates a new `Connection` in the current environment. The new connection will initially be disconnected.

### Environment.dataSources([type]) _(synchronous)_

Wrap **`SQLDataSources`**. Enumerates available data sources. `type` can be any of the following:

 * `"user"` lists all user DSNs.
 * `"system"` lists all system DSNs.
 * _(omitted)_: lists all user and system DSNs.

The data sources are returned as { server, description } pairs, e.g.
```json
[ { server: 'Products',
    description: 'Microsoft ODBC for Oracle' },
  { server: 'Customers',
    description: 'ODBC Driver 11 for SQL Server' } ]
```

The `server` property may be used as a DSN for `Connection.browseConnect`.

### Environment.free() _(synchronous)_

Destroys the environment handle.

### TODO

* Wrap **`SQLDrivers`** to enumerate available drivers.

## Connection

A `Connection` a wrapper around a `SQLHDBC` which is used to connect to data sources and create
statements to execute.

### Connection.connect(connectionString, callback)

Wraps **`SQLDriverConnect`**. Takes a complete connection string and connects to it. If any required connection string parameters are missing, the operation will fail.

### Connection.newStatement() _(synchronous)_

Creates a new `Statement` object, which can be used for preparing statements and executing SQL directly.

### Connection.disconnect(callback)

Disconnects from the data source. After a successful disconnect operation, the connection handle may be used again to connect to another data source.

### Connection.free() _(synchronous)_

Destroys the connection handle.
