var FS = require('fs'),
    Path = require('path'),
    Eos = require('../').bindings,
    Utils = require('util');

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

    conn: function _conn(cb) { 
        var conn = this.env.newConnection();
        conn.driverConnect(settings.connectionString, function(err) {
            if (err)
                return cb(err);
            cb(null, conn);
        });
    },

    stmt: function stmt(cb) {
        this.conn(function (err, cb) {
            if (err)
                return cb(err);
            var stmt = conn.newStatement();
            cb(null, stmt, conn);
        });
    },

    bufEqual: function bufEqual(x, y) {
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
    },

    closeTo: function closeTo(delta) {
        return function (x, y) {
            return Math.abs(x - y) < delta;
        };
    },
    
    pp: function pp(x) {
        x = Utils.inspect(x);
        if (x.length > 50)
            x = x.substr(0, 50) + "...";
        return x;
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
