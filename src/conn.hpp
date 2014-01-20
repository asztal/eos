#pragma once 

#include "eos.hpp"

namespace Eos {
    struct Connection : ObjectWrap {
        static void Init(Handle<Object> exports);

    private:
        static Persistent<FunctionTemplate> constructor_;
    };
}
