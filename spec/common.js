var FS = require('fs'),
    Path = require('path'),
    Eos = require('../');

var settings;
try {
    settings = require('./settings.user.json');
} catch (e) {
    settings = require('./settings.json');
}

var expect = require("chai").expect,
    should = require("chai").should;

module.exports = {
    settings: settings,

    expect: expect,
    should: should,

    env: new Eos.Environment(),

    conn: function (cb) { 
        var conn = this.env.newConnection();
        conn.connect(settings.connectionString, function(err) {
            if (err)
                return cb(err);
            cb(null, conn);
        });
    },

    stmt: function (cb) {
        this.conn(function (err, cb) {
            if (err)
                return cb(err);
            var stmt = conn.newStatement();
            cb(null, stmt, conn);
        });
    }
};

describe("Eos", function () {
    it("should export Environment", function () {
        expect(Eos.Environment).to.be.a("function");
    });

    it("should export OdbcError", function () {
        expect(Eos.OdbcError).to.be.a("function");
    });
});
