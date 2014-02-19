var assert = require('assert'),
    colors = require('colors'),
    domain = require('domain'),
    eos = require('./');

var connectionString = "Driver={ODBC Driver 11 for SQL Server};Server=.\\CONNECT;Trusted_Connection=Yes";

function log(x) { console.log(String(x).bold.yellow); }

var d = domain.create();

d.on("error", function (e) {
    console.log((((e && e.constructor && e.constructor.name) || "Error") + ":").red.bold, e);
});

d.run(function() {
    var env = new eos.Environment();

    var conn = env.newConnection();

    conn.connect(connectionString, d.bind(function (err) {
        log(err);
        assert.ifError(err);

        var stmt = conn.newStatement();
        log(stmt);

        stmt.execDirect("select 88 as x, 0x274269274269274269274269 as y, replicate('a',33) as z", d.bind(function (err) {
            assert.ifError(err);

            stmt.fetch(d.bind(function (err, hasData) {
                assert.ifError(err);
                log("hasData: " + hasData);

                var SQL_INTEGER = 4
                stmt.getData(1, SQL_INTEGER, null, false, function (err, result, totalBytes) {
                    assert.ifError(err);
                    log("result:" + typeof result);
                    console.log(result);
                    log("totalBytes:" + totalBytes);

                    conn.disconnect(d.bind(function (err) {
                        log(err);
                        assert.ifError(err);

                        conn.free();
                    }));
                });
            }));
        }));
    }));
});