var eos = require("../")

module.exports = function manyConcurrentFetches(conn, callback) {
    var stmt = conn.newStatement()

    stmt.bindParameter(1, eos.SQL_PARAM_INPUT, eos.SQL_INTEGER, 0, 0, Math.floor(Math.random() * 2048))

    function fail(err) {
        stmt.cancel()
        stmt.free()
        return callback(err)
    }
    
    stmt.execDirect("select number, name from master..spt_values where number < ?", function(err) {
        if (err)
            return fail(err)

        fetchLoop()

        function fetchLoop() {
            stmt.fetch(function(err, hasData) {
                if (err)
                    return fail(err)
                
                if (!hasData) {
                    stmt.cancel()
                    stmt.free()
                    return callback()
                }

                stmt.getData(1, eos.SQL_INTEGER, null, false, function(err, x) {
                    if (err)
                        return fail(err)
                    
                    stmt.getData(2, eos.SQL_VARCHAR, null, false, function(err, y) {
                        if (err)
                            return fail(err)
                        
                        fetchLoop()
                    })
                })
            })
        }
    })
}

module.exports.limit = 1
module.exports.times = 100
