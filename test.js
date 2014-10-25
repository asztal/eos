var assert = require('assert'),
    colors = require('colors'),
    domain = require('domain'),
    eos = require('./');

function log(x) { console.log(String(x).bold.yellow); }

if (true) {
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
