var eos = require('./');

try {
    var env = new eos.Environment();
    console.log(env, env.constructor, env.constructor.name)
} catch (e) {
    console.log("ERROR", e);
}