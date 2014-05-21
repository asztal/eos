var eos = require("../"),
    common = require("./common"),
    expect = common.expect,
    env = common.env,
    pp = common.pp,
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
            cmp = function (x, y) { return x === y; };

        describe("when binding " + Utils.inspect(val) + " as the first parameter", function () {
            it("should allow " + type + " and " + kind + (gdType ? " into " + gdType : ""), function (done) {
                stmt.bindParameter(1, eos[kind], eos[type], null, 0, val);

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
    
    var shortData = new Buffer([1, 2, 3, 4, 5, 6, 7, 8, 9], "binary");
    var longData = new Buffer(Math.floor(Math.random() * 20000) + 40000), longString = "";
    var shortString = "This is a string";
    for (var i = 0; i < longData.length; i++) {
        longData[i] = Math.floor(Math.random() * 256);
        longString += String.fromCharCode(Math.floor(Math.random() * 85) + 31);
    }

    testInputParam(shortData, "SQL_PARAM_INPUT", "SQL_BINARY", 0, common.bufEqual);

    // Fails (and rightly so)
    //testInputParam(longData, "SQL_PARAM_INPUT", "SQL_LONGVARBINARY", 0, common.bufEqual);

    function testOutputParam(sql, type, digits, _val, expected, buf, cmp) {
        if (!cmp)
            cmp = function (x, y) { return x === y; };

        describe("when executing " + sql, function () {
            it("should allow a value of type " + type, function (done) {
                var param = stmt.bindParameter(1, eos.SQL_PARAM_OUTPUT, eos[type], null, digits || 0, _val, buf);
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

                    var val = param.value;
                    if (!cmp(expected, val))
                        return done("Not equal: " + Utils.inspect(val) + " != " + Utils.inspect(expected));
                    
                    done();
                });
            });
        });
    }

    var undef = {}.x;
    testOutputParam("select ? = 42", "SQL_INTEGER", 0, undef, 42);
    testOutputParam("select ? = 42", "SQL_REAL", 0, undef, 42.0);
    testOutputParam("select ? = 'xyzzy'", "SQL_VARCHAR", 0, null, 'xyzzy', new Buffer(200));
    testOutputParam("select ? = 'xyzzy'", "SQL_WVARCHAR", 0, null, 'xyzzy', new Buffer(200));
    testOutputParam("select ? = N'This is a snowman: ☃'", "SQL_WVARCHAR", 0, null, 'This is a snowman: \u2603', new Buffer(200));
    testOutputParam("select ? = 0x010203040506070809", "SQL_VARBINARY", 0, null, shortData, new Buffer(200), common.bufEqual);

    function testInputOutputParam(sql, type, digits, val, expected, buf, cmp) {
        if (!cmp)
            cmp = function (x, y) { return x === y; };

        describe("when executing " + sql, function () {
            it("should allow a value of type " + type, function (done) {
                var param = stmt.bindParameter(1, eos.SQL_PARAM_INPUT_OUTPUT, eos[type], null, digits || 0, val, buf);

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

                        var result = param.value;
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
    testInputOutputParam("{call reverse(?)}", "SQL_WVARCHAR", 0, "forwards", "sdrawrof", new Buffer(200));

    function testDAEInputParam(val, type, len, digits, cmp, gdType) {
        if (!cmp)
            cmp = function (x, y) { return x === y; };

        describe("when binding " + Utils.inspect(val).substr(0,50) + " as the first parameter with data at execution", function () {
            it("should allow " + type + (gdType ? " into " + gdType : ""), function (done) {
                var param = stmt.bindParameter(1, eos.SQL_PARAM_INPUT, eos[type], len, 0);

                stmt.execDirect("select ? as x", function (err, needData, dataAvailable) {
                    if (err)
                        return done(err);
                    if (!needData)
                        return done("Expected needData to be true");
                    if (dataAvailable)
                        return done("Expected dataAvailable to be false");

                    stmt.paramData(function (err, dae) {
                        if (err)
                            return done(err);

                        if (!dae)
                            return done("Expected a DAE parameters to be required");
                        if (dae.index != param.index)
                            return done("Expected DAE parameter index to match input parameter");

                        param.value = val;

                        stmt.putData(param, function (err) {
                            if (err)
                                return done(err);

                            stmt.paramData(function (err, newDae) {
                                if (err)
                                    return done(err);

                                if (newDae)
                                    return done("Expected DAE for this parameter to be completed and no more parameters to be required");

                                stmt.fetch(function (err, hasData) {
                                    if (err)
                                        return done(err);
                                    if (!hasData)
                                        return done("No results");

                                    getDataLoop();

                                    function getDataLoop(fullResult) {
                                        stmt.getData(1, eos[gdType || type], null, false, function (err, result, _, more) {
                                            if (err)
                                                return done(err);

                                            if (typeof fullResult === "undefined")
                                                fullResult = result;
                                            else
                                                fullResult += result;

                                            if (more)
                                                return getDataLoop(fullResult);

                                            if (!cmp(fullResult, val)) {
                                                console.log(fullResult.length.toString().yellow.bold);
                                                console.log(val.length.toString().green.bold);
                                                return done("Not equal: " + pp(fullResult) + " != " + pp(val));
                                            }

                                            stmt.closeCursor();
                                            done();
                                        });
                                    }
                                });
                            });
                        });
                    });
                });
            });
        });
    }

    testDAEInputParam(27.42, "SQL_REAL", null, 0, common.closeTo(0.0001));
    testDAEInputParam(42.69, "SQL_REAL", null, 0, common.closeTo(0.7), "SQL_INTEGER");
    testDAEInputParam(69, "SQL_INTEGER", null, 0);
    testDAEInputParam(1142, "SQL_INTEGER", null, 0, null, "SQL_REAL");

    testDAEInputParam(shortData, "SQL_BINARY", shortData.length, 0, common.bufEqual);
    testDAEInputParam(shortString, "SQL_CHAR", shortString.length, 0);
    testDAEInputParam(shortString, "SQL_WCHAR", shortString.length, 0);

    testDAEInputParam(shortData, "SQL_VARBINARY", null, 0, common.bufEqual);
    testDAEInputParam(shortString, "SQL_VARCHAR", null, 0);
    testDAEInputParam(shortString, "SQL_WVARCHAR", null, 0);

    // Fails (and rightly so)
    // testDAEInputParam(longData, "SQL_BINARY", longData.length, 0, common.bufEqual);
    // testDAEInputParam(longString, "SQL_WCHAR", null, 0);

    testDAEInputParam(longData, "SQL_VARBINARY", null, 0, common.bufEqual);
    
    // Test long strings being split up and re-attached
    testDAEInputParam(longString + longString, "SQL_VARCHAR", null, 0);
    testDAEInputParam(longString, "SQL_WVARCHAR", null, 0);

    // Fails, most likely due to SQL Server's restrictions http://technet.microsoft.com/en-us/library/ms131031.aspx
    // testDAEInputParam(shortData, "SQL_LONGVARBINARY", null, 0, common.bufEqual);
    // testDAEInputParam(longData, "SQL_LONGVARBINARY", null, 0, common.bufEqual);

    // TODO this should fail, try it anyway
    // testDAEInputParam(longString, "SQL_WLONGVARCHAR", null, 0);

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
