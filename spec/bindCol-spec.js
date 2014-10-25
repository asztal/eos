var eos = require("../"),
    common = require("./common"),
    colors = require("colors"),
    expect = common.expect,
    env = common.env,
    pp = common.pp,
    Utils = require("util");

describe("Column binding", function(){
    var conn, stmt;
    
    beforeEach(function (done) {
        common.stmt(function (err, s, c) {
            if (err)
                return done(err);

            conn = c;
            stmt = s;
            done();
        });
    });

    it ("should work with some simple data", function(done) {
        var sql = "select 1 as id, 'Fred' as name, { d '1966-05-15' } as dob "
                + "union all select 2, 'Janet', { d '1984-12-11' } "
                + "union all select 3, 'Alex', { d '1992-05-25' }";

        var id = stmt.bindCol(1, eos.SQL_INTEGER),
            name = stmt.bindCol(2, eos.SQL_WVARCHAR, new Buffer(30)),
            dob = stmt.bindCol(3, eos.SQL_TYPE_DATE);
                
        stmt.execDirect(sql, function (err, needData, dataAvailable) {
            if (err)
                return done(err);
            if (needData || dataAvailable)
                return done(new Error("Unexpected SQL_NEED_DATA or SQL_PARAM_DATA_AVAILABLE"));

            fetchLoop(0);
            
            function fetchLoop(i){
                stmt.fetch(function(err, hasData){
                    if (err || !hasData)
                        return done(err);

                    var expected = [
                        [1, "Fred", new Date(1966, 04, 15)],
                        [2, "Janet", new Date(1984, 11, 11)],
                        [3, "Alex", new Date(1992, 04, 25)]    
                    ];

                    console.log(typeof expected[i][0], typeof id.value)
                    
                    if (expected[i][0] !== id.value)
                        return done(new Error(i + "th id wrong, expected " + expected[i][0] + ", got " + id.value));
                                    
                    if (expected[i][1] !== name.value)
                        return done(new Error(i + "th name wrong, expected " + expected[i][1] + ", got " + name.value));
                                    
                    if (expected[i][2].getTime() !== dob.value.getTime())
                        return done(new Error(i + "th dob wrong, expected " + expected[i][2] + ", got " + dob.value));
                                    
                    //console.log("Values".yellow.bold, id.value, name.value, dob.value);

                    fetchLoop(i + 1);
                });
            }        
        });
    });

    afterEach(function(){
        stmt.free();
        conn.disconnect(conn.free.bind(conn));
    });
});
