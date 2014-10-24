var eos = require("../")

module.exports = function execDirectSimple(conn, callback) {
    var stmt = conn.stmt = conn.stmt || conn.newStatement()

    if (!conn.buf1) {
        conn.buf1 = new Buffer(8)
        conn.buf2 = new Buffer(256)
        conn.buf3 = new Buffer(8)
        conn.buf4 = new Buffer(8)
        conn.buf5 = new Buffer(256)
    }

    stmt.bindParameter(1, eos.SQL_PARAM_INPUT, eos.SQL_INTEGER, 0, 0, Math.floor(Math.random() * 1000), conn.buf3)
    stmt.bindParameter(2, eos.SQL_PARAM_INPUT, eos.SQL_INTEGER, 0, 0, Math.floor(Math.random() * 1000), conn.buf4)
    stmt.bindParameter(3, eos.SQL_PARAM_INPUT, eos.SQL_VARCHAR, 0, 0, "This is a string", conn.buf5)
    
    stmt.execDirect("select ? + ?, ?", function(err) {
        if (err)
            return callback(err)

        stmt.fetch(function(err, hasData) {
            if (err)
                return callback(err)
            
            stmt.getData(1, eos.SQL_INTEGER, conn.buf1, false, function(err, x) {
                if (err)
                    return callback(err)

                stmt.getData(2, eos.SQL_VARCHAR, conn.buf2, false, function(err, y) {
                    if (err)
                        return callback(err)

                    stmt.closeCursor()

                    return callback()
                })
            })
        })
    })
}

module.exports.limit = 1


