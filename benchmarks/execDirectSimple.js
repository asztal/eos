var eos = require("../")

module.exports = function execDirectSimple(conn, callback) {
    var stmt = conn.stmt = conn.stmt || conn.newStatement();
    
    if (!conn.x) {
        stmt.prepare("select ? + ?, ?", exec);
        
        conn.x = stmt.bindParameter(1, eos.SQL_PARAM_INPUT, eos.SQL_INTEGER, 0, 0, Math.floor(Math.random() * 1000), conn.buf3);
        conn.y = stmt.bindParameter(2, eos.SQL_PARAM_INPUT, eos.SQL_INTEGER, 0, 0, Math.floor(Math.random() * 1000), conn.buf4);
        conn.str = stmt.bindParameter(3, eos.SQL_PARAM_INPUT, eos.SQL_VARCHAR, 30, 0, "This is a string", conn.buf5);

        conn.z = stmt.bindCol(1, eos.SQL_INTEGER);
        conn.res = stmt.bindCol(2, eos.SQL_WVARCHAR, 30);
    } else {
        exec();
    }

    function exec(err) {
        if (err)
            return callback(err);

        conn.x.value = Math.floor(Math.random() * 1000);
        conn.y.value = Math.floor(Math.random() * 1000);
        conn.z.value = "This is a string";
        
        stmt.execute(function(err) {
            if (err)
                return callback(err);
            
            stmt.fetch(function(err, hasData) {
                if (err)
                    return callback(err);

                //console.log(conn.z.value, conn.res.value);
                stmt.closeCursor();
                
                return callback();
            });
        });
    };
};

module.exports.limit = 1;



