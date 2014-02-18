var eos = require("../"),
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
});
