#pragma once 

#include "eos.hpp"
#include "handle.hpp"

namespace Eos {
    struct Environment: EosHandle {
        Environment(SQLHENV hEnv);
        ~Environment();

        static void Init(Handle<Object> exports);
        static NAN_METHOD(New);

    public:
        // JS methods
        NAN_METHOD(NewConnection);
        NAN_METHOD(DataSources);
        NAN_METHOD(Drivers);

    public:
        // Non-JS methods
        static Handle<FunctionTemplate> Constructor() { return NanNew(constructor_); }

    private:
        static Persistent<FunctionTemplate> constructor_;
    };
}