var assert = require('assert')
  , colors = require('colors')
  , domain = require('domain')
  , eos = require('./');

var connectionString = "Driver={ODBC Driver 11 for SQL Server};Server=.\\CONNECT;Trusted_Connection=Yes";

function log(x) { console.log(String(x).bold.yellow); }

var d = domain.create();

d.on("error", function (e) {
    console.log((((e && e.constructor && e.constructor.name) || "Error") + ":").red.bold, e);
});

d.run(function() {
    var env = new eos.Environment();

    var conn = env.newConnection();

    conn.browseConnect("DRIVER={ODBC Driver 11 for SQL Server}", function (err, more, outConnStr) {
        if (err) {
            console.log(err);
            return;
        }

        console.log(outConnStr);

    });

    return;
    conn.connect(connectionString, d.bind(function (err) {
        log(err);
        assert.ifError(err);

        var stmt = conn.newStatement();
        log(stmt);

        conn.disconnect(d.bind(function (err) {
            log(err);
            assert.ifError(err);

            conn.free();
        }));
    }));
});