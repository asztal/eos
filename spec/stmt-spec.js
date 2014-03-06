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

    function testInputParam(val, kind, type, digits, cmp, gdType) {
        if (!cmp)
            cmp = function (x, y) { return x === y };

        describe("when binding " + Utils.inspect(val) + " as the first parameter", function () {
            it("should allow " + type + " and " + kind + (gdType ? " into " + gdType : ""), function (done) {
                stmt.bindParameter(1, eos[kind], eos[type], 0, val);

                stmt.execDirect("select ? as x", function (err, needData, dataAvailable) {
                    if (err)
                        return done(err);
                    if (needData)
                        return done("Expected needData to be false");
                    if (dataAvailable)
                        return done("Expected dataAvailable to be false");

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

    testInputParam("xyzzy", "SQL_PARAM_INPUT", "SQL_VARCHAR", 0, null, "SQL_VARCHAR");
    testInputParam("xyzzy", "SQL_PARAM_INPUT", "SQL_VARCHAR", 0, null, "SQL_WVARCHAR");
    testInputParam("xyzzy", "SQL_PARAM_INPUT", "SQL_WVARCHAR", 0, null, "SQL_VARCHAR");
    testInputParam("xyzzy", "SQL_PARAM_INPUT", "SQL_WVARCHAR", 0, null, "SQL_WVARCHAR");
    
    testInputParam(42, "SQL_PARAM_INPUT", "SQL_INTEGER", 0);
    testInputParam(27.69, "SQL_PARAM_INPUT", "SQL_REAL", 2, common.closeTo(0.0001));
    testInputParam(27.69, "SQL_PARAM_INPUT", "SQL_DOUBLE", 2, common.closeTo(0.0001));
    testInputParam(27.69, "SQL_PARAM_INPUT", "SQL_FLOAT", 2, common.closeTo(0.0001));
    testInputParam(27.69, "SQL_PARAM_INPUT", "SQL_INTEGER", 2, common.closeTo(0.7));
    
    var data = new Buffer([1,2,3,4,5,6,7,8,9], "binary");
    testInputParam(data, "SQL_PARAM_INPUT", "SQL_BINARY", 0, common.bufEqual);

    function testOutputParam(sql, type, digits, _val, expected, buf, cmp) {
        if (!cmp)
            cmp = function (x, y) { return x === y; };

        describe("when executing " + sql, function () {
            it("should allow a value of type " + type, function (done) {
                var param = stmt.bindParameter(1, eos.SQL_PARAM_OUTPUT, eos[type], digits || 0, _val, buf);
                stmt.execDirect(sql, function (err, needData, hasData) {
                    if (err)
                        return done(err);
                    if (needData)
                        return done("Expected needData to be false");
                    if (hasData)
                        return done("Expected hasData to be false");

                    if (param.index != 1)
                        return done("Parameter index is wrong: " + param.index);
                    if (param.kind != eos.SQL_PARAM_OUTPUT)
                        return done("Parameter kind is wrong");

                    var val = param.getValue();
                    if (!cmp(expected, val))
                        return done("Not equal: " + Utils.inspect(val) + " != " + Utils.inspect(expected));
                    
                    done();
                });
            });
        });
    }

    var undefined = {}.x;
    testOutputParam("select ? = 42", "SQL_INTEGER", 0, undefined, 42);
    testOutputParam("select ? = 42", "SQL_REAL", 0, undefined, 42.0);
    testOutputParam("select ? = 'xyzzy'", "SQL_VARCHAR", 0, null, 'xyzzy', new Buffer(200));
    testOutputParam("select ? = N'This is a snowman: ☃'", "SQL_WVARCHAR", 0, null, 'This is a snowman: \u2603', new Buffer(200));
    testOutputParam("select ? = 0x010203040506070809", "SQL_VARBINARY", 0, null, data, new Buffer(200), common.bufEqual);

    function testInputOutputParam(sql, type, digits, val, expected, buf, cmp) {
        if (!cmp)
            cmp = function (x, y) { return x === y; };

        describe("when executing " + sql, function () {
            it("should allow a value of type " + type, function (done) {
                var param = stmt.bindParameter(1, eos.SQL_PARAM_INPUT_OUTPUT, eos[type], digits || 0, val, buf);

                stmt.setParameterName(1, "@x");

                stmt.execDirect(sql, function (err, needData, hasData) {
                    if (err)
                        return done(err);
                    if (needData)
                        return done("Expected needData to be false");
                    if (hasData)
                        return done("Expected hasData to be false");

                    if (param.index != 1)
                        return done("Parameter index is wrong: " + param.index);
                    if (param.kind != eos.SQL_PARAM_INPUT_OUTPUT)
                        return done("Parameter kind is wrong");

                        var result = param.getValue();
                        if (!cmp(expected, result))
                            return done("Not equal: " + Utils.inspect(result) + " != " + Utils.inspect(expected));

                        done();

                    //stmt.fetch(function (err, needData, hasData) {
                    //    if (needData)
                    //        return done("Expected needData to be false");
                    //    if (hasData)
                    //        return done("Expected hasData to be false");

                    //});
                });
            });
        });
    }

    testInputOutputParam("{call increment(?)}", "SQL_INTEGER", 0, 42, 43, new Buffer(200));
    testInputOutputParam("{call reverse(?)}", "SQL_VARCHAR", 0, "forwards", "sdrawrof", new Buffer(200));

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
