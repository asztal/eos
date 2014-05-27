var eos = require("../"),
    common = require("./common"),
    colors = require("colors"),
    expect = common.expect;

function git(desc, g) {
    it(desc, eos.run(g));
}

var xgit = xit;

function newConn() {
    var e = new eos.Environment();
    return e.newConnection();
}

function* newConnected() {
    var e = new eos.Environment(),
        c = e.newConnection();
    yield c.driverConnect(common.settings.connectionString);
    return c;
}

function expectError(state, f) {
    try {
        f();
    } catch (ex) {
        if (ex.state != state)
            throw ex;
        return;
    }
    throw new Error("Expected error with state " + state);
}

function* gExpectError(state, f) {
    try {
        yield f();
    } catch (ex) {
        if (ex.state != state)
            throw ex;
        return;
    }
    throw new Error("Expected error with state " + state);
}

describe("Environment", function () {
    it("should construct and destruct successfully", function () {
        var e = new eos.Environment();
        e.free();
    });

    it("should not destruct while a connection exists", function () {
        var e = new eos.Environment(),
            c = e.newConnection();
        
        // Function sequence error
        expectError("HY010", function() {
            e.free();
        });
    });
});

describe("Connection", function () {
    it("should construct and destruct successfully", function () {
        var e = new eos.Environment(),
            c = e.newConnection();
        c.free();
        e.free();
    });

    it("should not allow creating statements before connecting", function () {
        var e = new eos.Environment(),
            c = e.newConnection();
        
        // Connection not open
        expectError("08003", function() {
            s = c.newStatement();
        });
    });

    git("should connect to " + common.settings.connectionString, function*() {
        var c = newConn();
        yield c.driverConnect(common.settings.connectionString);
    });
});

describe("A connected connection", function() {
    git("should allow creating statements", function* () {
        var c = yield newConnected(),
            s = c.newStatement();

        s.free();
        yield c.disconnect();
        c.free();
    });
});

describe("Finally...", function() {
    it("there should be no active operations", function() {
        if(eos.bindings.activeOperations && eos.bindings.activeOperations().length > 0) {
            var ops = eos.activeOperations();

            console.log("Active operations:".green.bold);
            console.log("------------------\n".green.bold);
            for (var i = 0; i < ops.length; i++) {
                console.log(ops[i].name.magenta.bold, "(refs: " + ops[i].refs.toString().yellow.bold + "):");
                console.log(ops[i].stackTrace);
            }

            throw new Error("Active operations exist");
        }
    });
});