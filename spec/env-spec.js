var eos = require("../"),
    common = require("./common"),
    expect = common.expect;

describe("Environment", function () {
    it("should construct successfully", function () {
        new eos.Environment();
    });

    it("should destruct successfully", function () {
        new eos.Environment().free();
    });

    describe("Environment.drivers", function () {
        it("should return an array", function () {
            var drivers = common.env.drivers();
            expect(drivers).to.be.an(Array);
            expect(drivers).not.to.be.empty();
            drivers.forEach(function (driver) {
                expect(driver).to.be.an(Object);
                expect(driver).to.only.have.keys("name", "attributes");
                driver.attributes.forEach(function (attribute) {
                    expect(attribute).to.be.a("string");
                    expect(attribute).to.contain("=");
                });
            });
        });

        it("should return each driver only once", function () {
            var drivers = common.env.drivers();
            for (var i = 0; i < drivers.length; i++)
                for (var j = 0; j < i; j++)
                    if (drivers[i].name == drivers[j].name)
                        throw "Driver name occurred twice: " + drivers[j].name;
        });
    });

    describe("Environment.dataSources", function () {
        it("should return an array", function () {
            var dataSources = common.env.dataSources();
            expect(dataSources).to.be.an(Array);
            expect(dataSources).not.to.be.empty();
            dataSources.forEach(function (dsn) {
                expect(dsn).to.be.an(Object);
                expect(dsn).to.only.have.keys("server", "description");
                expect(dsn.server).to.be.a('string');
                expect(dsn.description).to.be.a('string');
            });
        });

        it("should return each data source only once", function () {
            var dataSources = common.env.dataSources();
            for (var i = 0; i < dataSources.length; i++)
                for (var j = 0; j < i; j++)
                    if (dataSources[i].server == dataSources[j].server)
                        throw "Data source name occurred twice: " + dataSources[j].server;
        });

        it("should accept 'user' as a parameter", function () {
            var all = common.env.dataSources(),
                user = common.env.dataSources('user');
            expect(user.length).not.to.be.greaterThan(all.length);
        });

        it("should accept 'system' as a parameter", function () {
            var all = common.env.dataSources(),
                system = common.env.dataSources('system');
            expect(system.length).not.to.be.greaterThan(all.length);
        });

        it("the 'user' and 'system' lists should add up to the total list", function () {
            var all = common.env.dataSources(),
                user = common.env.dataSources('user'),
                system = common.env.dataSources('system');
            expect(user.length + system.length).to.be(all.length);
        });
    });
});
