var eos = require('./');

try {
    var env = new eos.Environment();
    console.log("env: ", env.constructor, env.constructor.name);

    env.free();
    env.free();

    var conn = env.newConnection();
    console.log("conn: ", conn.constructor, conn.constructor.name);

    var C = conn.constructor;

    var conn2 = new C(env);
    console.log("conn2: ", conn2.constructor, conn2.constructor.name);


} catch (e) {
    console.log("ERROR", typeof e, e, e.constructor);
}