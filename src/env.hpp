#pragma once 

#include "eos.hpp"
#include "handle.hpp"

namespace Eos {
    struct Environment: EosHandle {
        Environment(SQLHENV hEnv);
        ~Environment();

        static void Init(Handle<Object> exports);
        static Handle<Value> New(const Arguments& args);

    public:
        // JS methods
        Handle<Value> NewConnection(const Arguments& args);
        Handle<Value> DataSources(const Arguments& args);
        Handle<Value> Drivers(const Arguments& args);

    public:
        // Non-JS methods
        static Persistent<FunctionTemplate> Constructor() { return constructor_; }

    private:
        static Persistent<FunctionTemplate> constructor_;
    };
}