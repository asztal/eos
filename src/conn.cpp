#include "conn.hpp"

using namespace Eos;

Persistent<FunctionTemplate> Connection::constructor_;

void Connection::Init(Handle<Object> exports)  {
    EOS_DEBUG_METHOD();
}

namespace { ClassInitializer<Connection> c; }