var bindings = require("./bindings"),
    assert = require("assert"),
    co = require("co");

var eos = module.exports = {
    bindings: bindings,
    run: co
};

function Environment() {
    this.handle = new bindings.Environment();
}

eos.Environment = Environment;

Environment.prototype.free = function () {
    this.handle.free();
};

Environment.prototype.newConnection = function () {
    return new Connection(this);
};

function Connection(env) {
    this.env = env;
    this.handle = env.handle.newConnection();
}

Connection.prototype.connect = function (serverName, userName, password) {
    return function (callback) {
        this.handle.connect(serverName, userName, password, callback);
    };
};

function makeConnectionString(obj) {
    if (typeof obj === 'string')
        return obj;

    var str = "";
    for (var key in obj)
        str += (str ? ";" : "") + key + "=" + obj[key];
    return str;
}

Connection.prototype.driverConnect = function (connectionString) {
    var self = this;
    return function (callback) {
        self.handle.driverConnect(makeConnectionString(connectionString), callback);
    };
};

Connection.prototype.browseConnect = function (connectionString) {
    var self = this;
    return function (callback) {
        self.handle.browseConnect(makeConnectionString(connectionString), function (err, result, needData) {
            if (err)
                return callback(err);

            callback(null, {
                result: result,
                needData: needData
            });
        });
    };
};

/*
  connection-string ::= attribute[;] | attribute; connection-string
  attribute ::= [*]attribute-keyword=attribute-value
  attribute-keyword ::= ODBC-attribute-keyword | driver-defined-attribute-keyword
  ODBC-attribute-keyword = {UID | PWD}[:localized-identifier]
  driver-defined-attribute-keyword ::= identifier[:localized-identifier]
  attribute-value ::= {attribute-value-list} | ? (The braces are literal; they are returned by the driver.)
  attribute-value-list ::= character-string [:localized-character string] | character-string [:localized-character string], attribute-value-list
*/
function parseConnectionString(str) {
    return str.split(';').map(function(attr) {
        var p = attr.split('='),
            q = p[0].split(':'),
            name = q[0],
            label = q[1],
            value = p[1],
            optional = false;

        if (name[0] == "*") {
            optional = true;
            name = name.substring(1);
        }

        var part = {
            name: name
        };

        if (label)
            part.label = label;

        if (optional)
            part.optional = true;

        if (value != '?' && value[0] == '{' && value[value.length - 1] == '}')
            part.options = value.substr(1, value.length - 2).split(',');

        return part;
    });
}

Connection.prototype.browseConnectLoop = function*(connectionString, requestUpdates) {
    function hasValue(u) { return !!u.value; }
    function toAttribute(u) { return u.name + "=" + u.value; }

    while (true) {
        var rv = yield this.browseConnect(connectionString);

        if (!rv.needData)
            return x.result;
        
        var request = parseConnectionString(rv.result),
            updates = requestUpdates(request, rv.result);
        
        if (!updates) {
            yield this.disconnect();
            return;
        }

        connectionString = updates.filter(hasValue).map(toAttribute).join(";");
    }
};

Connection.prototype.disconnect = function () {
    var self = this;
    return function (callback) {
        self.handle.disconnect(callback);
    };
};

Connection.prototype.nativeSql = function (sql) {
    return this.handle.nativeSql(sql);
};

Connection.prototype.free = function () {
    this.handle.free();
};

Connection.prototype.newStatement = function () {
    return new Statement(this);
};

//Conn.prototype.prepare = function (sql /*, parameters..., callback*/) {
//    var stmt = new Stmt(this);

//    var cb = arguments[arguments.length - 1];
//    assert(cb instanceof Function);

//    var params = Array.prototype.slice.call(arguments, 1, -1);
//    stmt.prepare(sql, function (err) {
//        if (err)
//            return cb(err);

//        try {
//            stmt.bind(parameters);
//        } catch (e) {

//        }

//        cb(null, stmt);
//    });

//    return stmt;
//};

function Statement(conn) {
    this.conn = conn;
    this.handle = conn.handle.newStatement();
}

Statement.prototype.free = function () {
    this.handle.free();
};

Statement.prototype.closeCursor = function (canThrow) {
    this.handle.closeCursor(canThrow);
};

module.exports.internals = { parseConnectionString: parseConnectionString };
