var assert = require('assert'),
    colors = require('colors'),
    domain = require('domain'),
    eos = require('./');

function log(x) { console.log(String(x).bold.yellow); }

if (false) {
    var jasmine = require("jasmine-node");
    jasmine.executeSpecsInFolder({
        specFolders: ["spec"],
        showColors: true,
        isVerbose: true
    });
} else {
    var connectionString = "Driver={ODBC Driver 11 for SQL Server};Server=.\\CONNECT;Trusted_Connection=Yes";
    
    test();

    // var d = domain.create();
    // 
    // d.on("error", function (e) {
    //     console.log((((e && e.constructor && e.constructor.name) || "Error") + ":").red.bold, e);
    // });
    // 
    // d.run(test);
}

function test() {
    var env = new eos.Environment(),
        conn = env.newConnection();

    conn.driverConnect(require("./spec/settings.json").connectionString, function () {
        conn.disconnect(function () {
            conn.free();

            console.log("Active handles:".bold.yellow);
            eos.activeHandles().forEach(function (handle) {
                console.log(" *".bold.yellow, handle.constructor.name.bold.white);
            });

            console.log("Active operations:".bold.yellow);
            eos.activeOperations().forEach(function (operation) {
                console.log(" *".bold.yellow, handle.constructor.name.bold.white);
            });
        });
    });

    return;
    conn.driverConnect(require("./spec/settings.json").connectionString, function(err) {
        if (err)
            throw err;

        var stmt = conn.newStatement();

        stmt.prepare("select 42 as x", function () {
            var t0 = new Date().getTime();

            function donex(a, e) {
                console.log("I DONE IT".bold.red, a, e);
                console.trace();
                while (1)
                    console.log("I R BABOON");
                return donex(a, e);
            }

            stmt.execute(function (err) {
                var t1 = new Date().getTime();

                console.log("t1: %s", (t1 - t0).toString().bold.yellow);


                if (err) {
                    console.log("Err!!!!!!!!!!!", err);
                    process.exit(999);
                    return done(err);
                }

                stmt.closeCursor();

                var t2 = new Date().getTime();

                console.log("t2: %s", (t2 - t1).toString().bold.yellow);

                stmt.execute(function (e, r) {
                    var t3 = new Date().getTime();

                    console.log("t3: %s", (t3 - t2).toString().bold.yellow);

                    console.log("FINISHED OMG".green.bold);
                    stmt.free();
                    conn.disconnect(function () {
                        conn.free();
                        env.free();

                        delete env;
                        delete conn;
                        delete stmt;

                        gc();

                        debugOps();
                    });
                });
            });
        });
    })
}

function debugOps() {
    var ops = eos.bindings.activeOperations();
    console.log("Active operations:".green.bold, ops.length);
    console.log("------------------\n".green.bold);
    for (var i = 0; i < ops.length; i++) {
        console.log(ops[i].name.magenta.bold + "(refs: %s, begun: %s, completed: %s, sync: %s, result: %s):",
            ops[i].refs.toString().yellow.bold,
            (typeof ops[i].begun !== "undefined" ? ops[i].begun : "wot").toString().yellow.bold,
            (typeof ops[i].completed !== "undefined" ? ops[i].completed : "wat").toString().yellow.bold,
            (typeof ops[i].sync !== "undefined" ? ops[i].sync : "uwotm8").toString().yellow.bold,
            (typeof ops[i].result !== "undefined" ? ops[i].result : "wut").toString().yellow.bold);

        console.log(ops[i].stackTrace);
    }
}

function testx() {
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

                var SQL_INTEGER = 4, SQL_VARCHAR = 12, SQL_BINARY = -2;
                var SlowBuffer = require('buffer').SlowBuffer;

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
}

