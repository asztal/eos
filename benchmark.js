var eos = require("./"),
    FS = require("fs"),
    Path = require("path"),
    Async = require("async")

require("colors")

var Cluster = require("cluster")
//for (var i = 0; i < 4; i++)
  //  if (Cluster.isMaster)
    //    require("cluster").fork()

var connectionString = "Driver={ODBC Driver 11 for SQL Server};Server=localhost,3800;Trusted_Connection=Yes"

function repeatLimit(fn, limit, times, callback) {
    var start = new Date().getTime(), iterations = 0, active = 0, started = 0, finished = 0, failed = false;

    var env = new eos.Environment(),
        connections = []

    for (var i = 0; i < limit; i++)
        connections.push(i)

    Async.mapLimit(connections, 16, function(_, cb) {
        var conn = env.newConnection()
        conn.driverConnect(connectionString, function(err) {
            return cb(err, !err && conn);
        })
    }, function(err, results) {
        if (err) {
            return callback(err)
            
            connections.forEach(function(c) {
                c.disconnect(function() {
                    c.free()
                })
            })
        }
        
        connections = results
        
        for (var i = 0; i < limit; i++)
            begin(i)
    })

    function begin(i) {
        if (started >= times)
            return

        started++
        fn(connections[i], function(err) {
            finished++
            
            if (err) {
                if (!failed) {
                    failed = true
                    callback(err)
                }
                return
            }
            
            if (finished >= times) {
                callback()
                callback = null
            } else if(started < times) {
                begin(i)
            } else {
                connections[i].disconnect(function() {
                    connections[i].free()
                })
            }
        })
    }
}

Async.eachSeries(FS.readdirSync(Path.join(__dirname, "benchmarks")), function(name, callback) {
    if (!/\.js$/.test(name))
        return callback()

    var fn = require(Path.join(__dirname, "benchmarks", name))
    console.log("Running benchmark", fn.name.bold.magenta)
    
    var start = new Date().getTime(),
        times = fn.times || 10000
    
    repeatLimit(fn, fn.limit || 1, times, function(err) {
        callback(err)

        if (!err) {
            var elapsed = new Date().getTime() - start
            console.log("Benchmark", fn.name.bold.magenta,
                        "run", times.toString().yellow.bold,
                        "times in", elapsed.toString().yellow.bold + "ms,",
                        Math.floor(times/(elapsed/1000)).toString().yellow.bold + "/sec")
        }
    })
}, function(err) {
    if (err)
        console.error(err.message)
})

