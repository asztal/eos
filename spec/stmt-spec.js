var eos = require("../"),
    common = require("./common"),
    expect = common.expect,
    env = common.env,
    Utils = require("util");

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

    function test(val, kind, type, digits, cmp, gdType) {
        if (!cmp)
            cmp = function (x, y) { return x === y };

        describe("when binding " + Utils.inspect(val) + " as the first parameter", function () {
            it("should allow " + type + " and " + kind + (gdType ? " into " + gdType : ""), function (done) {
                stmt.bindParameter(1, eos[kind], eos[type], 0, val);

                stmt.execDirect("select ? as x", function (err) {
                    if (err)
                        return done(err);

                    stmt.fetch(function (err, hasData) {
                        if (err)
                            return done(err);
                        if (!hasData)
                            return done("No results");

                        stmt.getData(1, eos[gdType || type], null, false, function (err, result) {
                            if (err)
                                return done(err);

                            if (!cmp(result, val))
                                return done("Not equal: " + result + " != " + val);

                            stmt.closeCursor();
                            done();
                        });
                    });
                });
            });
        });
    }

    test("xyzzy", "SQL_PARAM_INPUT", "SQL_VARCHAR", 0, null, "SQL_VARCHAR");
    test("xyzzy", "SQL_PARAM_INPUT", "SQL_VARCHAR", 0, null, "SQL_WVARCHAR");
    test("xyzzy", "SQL_PARAM_INPUT", "SQL_WVARCHAR", 0, null, "SQL_VARCHAR");
    test("xyzzy", "SQL_PARAM_INPUT", "SQL_WVARCHAR", 0, null, "SQL_WVARCHAR");

    function closeTo(delta) {
        return function (x, y) {
            return Math.abs(x - y) < delta;
        }
    }

    test(42, "SQL_PARAM_INPUT", "SQL_INTEGER", 0);
    test(27.69, "SQL_PARAM_INPUT", "SQL_REAL", 2, closeTo(0.0001));
    test(27.69, "SQL_PARAM_INPUT", "SQL_DOUBLE", 2, closeTo(0.0001));
    test(27.69, "SQL_PARAM_INPUT", "SQL_FLOAT", 2, closeTo(0.0001));
    test(27.69, "SQL_PARAM_INPUT", "SQL_INTEGER", 2, closeTo(0.7));

    function bufEqual(x, y) {
        if (!x && !y)
            return true;
        if (!x || !y)
            return false;
        if (x.length !== y.length)
            return false;
        for (var i = 0; i < x.length; i++)
            if (x[i] !== y[i])
                return false;
        return true;
    }

    var data = new Buffer([1,2,3,4,5,6,7,8,9], "binary");
    test(data, "SQL_PARAM_INPUT", "SQL_BINARY", 0, bufEqual);

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
