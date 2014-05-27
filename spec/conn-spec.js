var eos = require("../").bindings,
    common = require("./common"),
    expect = common.expect,
    env = common.env;

describe("A newly created connection", function () {
    it("should construct and free successfully", function () {
        var conn = env.newConnection();
    });

    it("should construct by using the constructor with an Environment", function () {
        var c = env.newConnection();
        var Connection = env.constructor;
        c.free();
        c = new Connection(env);
        c.free();
    });

    it("should fail when trying to disconnect", function (done) {
        var c = env.newConnection();
        c.disconnect(function (err) {
            err = err ? null : "Expected error";
            done(err);
        });
    });

    it("should fail when trying to create a statement", function () {
        var c = env.newConnection();
        expect(function () { return c.newStatement(); }).to.throw(eos.OdbcError);
    });

    it("should connect to common.connectionString", function (done) {
        common.conn(done);
    });

    it("should connect using SQLConnect using 1 parameter", function (done) {
        var c = env.newConnection();
        c.connect(common.settings.dsn, done);
    });

    it("should connect using SQLConnect using 1 parameter and null", function (done) {
        var c = env.newConnection();
        c.connect(common.settings.dsn, null, done);
    });

    it("should connect using SQLConnect using 2 parameters and null", function (done) {
        var c = env.newConnection();
        c.connect(common.settings.dsn, null, null, done);
    });

    describe("nativeSql", function () {
        it("should fail", function () {
            try {
                env.newConnection().nativeSql("{call increment(?)}");
            } catch (e) {
                if (e instanceof eos.OdbcError)
                    return;
                throw e;
            }
            throw "Didn't throw";
        });
    });
});

describe("For a connected connection", function () {
    var conn;
    
    beforeEach(function (done) {
        common.conn(function (err, c) {
            if (err)
                return done(err);
            conn = c;
            done();
        });
    });

    describe("nativeSql", function () {
        it("should succeed with an ODBC call statement", function () {
            conn.nativeSql("{call increment(?)}");
        });
        it("should succeed with an ODBC select statement", function () {
            conn.nativeSql("select { fn CONVERT (27, SQL_SMALLINT) }");
        });
    });

    afterEach(function () {
        conn.disconnect(conn.free.bind(conn));
    });
});