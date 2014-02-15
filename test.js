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

        stmt.execDirect("select 42.0 as x, 0x274269274269274269274269 as y, replicate('a',33) as z", d.bind(function (err) {
            assert.ifError(err);

            stmt.fetch(d.bind(function (err, hasData) {
                assert.ifError(err);
                log("hasData: " + hasData);

                stmt.numResultCols(d.bind(function (err, cols) {
                    assert.ifError(err);
                    log("cols: " + cols);

                    stmt.describeCol(2, d.bind(function (err, name, type, chars, digits, nullable) {
                        assert.ifError(err);

                        log("name: " + name);
                        log("type: " + type);
                        log("chars: " + chars);
                        log("digits: " + digits);
                        log("nullable: " + nullable);

                        conn.disconnect(d.bind(function (err) {
                            log(err);
                            assert.ifError(err);

                            conn.free();
                        }));
                    }));
                }));
            }));
        }));
    }));
});