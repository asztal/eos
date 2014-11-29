try {
    module.exports = require("../build/Debug/eos.node");
} catch (e) {
    //var fs = require("fs")
    //fs.writeFileSync(__dirname + "/../build/Release/eos.node", fs.readFileSync(__dirname + "/../build/Release/eos.dll"))
    module.exports = require("../build/Release/eos.node");
}
