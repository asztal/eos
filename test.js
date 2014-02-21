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

        stmt.execDirect("select 88 as x, 'I shot the sheriff' as y, replicate('a',33) as z", d.bind(function (err) {
            assert.ifError(err);

            stmt.fetch(d.bind(function (err, hasData) {
                assert.ifError(err);
                log("hasData: " + hasData);

                var SQL_INTEGER = 4, SQL_VARCHAR = 12, SQL_BINARY = -2

                var SlowBuffer = require('buffer').SlowBuffer

                stmt.getData(2, SQL_BINARY, new SlowBuffer(100), false, function (err, result, totalBytes) {
                    assert.ifError(err);
                    log("result:" + typeof result);
                    console.log(result);
                    console.log(result.parent);
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
