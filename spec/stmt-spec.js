var eos = require("../"),
    common = require("./common"),
    expect = common.expect,
    env = common.env;

describe("A newly created statement", function () {
    var conn, stmt;

    beforeEach(function (done) {
        common.conn(function (err, c) {
            if (err)
                return done(err);

            conn = c;
            stmt = c.newStatement();
            done();
        });
    });

    it("should allow prepare", function (done) {
        stmt.prepare("select 123 as foo, 'abc' as bar, null as baz", done);
    });

    it("should allow execDirect", function (done) {
        stmt.execDirect("select 123 as foo, 'abc' as bar, null as baz", done);
    });

    it("should not allow execute", function (done) {
        stmt.execute(function (err) {
            done(err ? null : "Expected error");
        });
    });

    afterEach(function () {
        stmt.free();
        conn.disconnect(conn.free.bind(conn));
    });
});

describe("A prepared statement", function () {
    var conn, stmt;

    beforeEach(function (done) {
        common.conn(function (err, c) {
            if (err)
                return done(err);

            conn = c;
            stmt = c.newStatement();
            stmt.prepare("select 42 as x", done);
        });
    });

    it("should allow executing", function (done) {
        stmt.execute(done);
    });

    it("should allow executing, cancelling, then executing", function (done) {
        stmt.execute(function (err) {
            if (err)
                return done(err);
            stmt.cancel();
            stmt.execute(done);
        });
    });

    afterEach(function () {
        stmt.free();
        conn.disconnect(conn.free.bind(conn));
    });
});
