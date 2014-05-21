try {
    module.exports = require("../build/Debug/eos.node");
} catch (e) {
    module.exports = require("../build/Release/eos.node");
}
