var assert = require('assert')
  , colors = require('colors')
  , eos = require('./');

var connectionString = "Driver={ODBC Driver 11 for SQL Server};Server=.\\CONNECT;Trusted_Connection=Yes";

try {
    var env = new eos.Environment();
    var conn = env.newConnection();

    conn.connect(connectionString, function (err) {
        console.log(err);
        assert.ifError(err);

        console.log("WAT");
    })

} catch (e) {
    console.log((((e && e.constructor && e.constructor.name) || "Error") + ":").red.bold, e);
}