#include "buffer.hpp"
#include <ctime>

namespace Eos {
    namespace Buffers {
        bool Allocate(SQLLEN length, SQLPOINTER& buffer, Handle<Object>& handle) {
            handle = JSBuffer::New(length);
            if (handle.IsEmpty()) {
                EOS_DEBUG(L"Failed to allocate parameter buffer\n");
                buffer = nullptr;
                return false;
            }

            JSBuffer::Unwrap(handle, buffer, length);
            return true;
        }

        SQLLEN GetDesiredBufferLength(SQLSMALLINT cType) {
            switch(cType) {
            case SQL_C_SLONG: return sizeof(long);
            case SQL_C_DOUBLE: return sizeof(double);
            case SQL_C_BIT: return sizeof(bool);
            case SQL_C_TYPE_TIMESTAMP: return sizeof(SQL_TIMESTAMP_STRUCT);
            default: return 0;
            }
        }

        SQLLEN FillInputBuffer(SQLSMALLINT cType, Handle<Value> jsValue, SQLPOINTER buffer, SQLLEN length) {
            switch(cType) {
            case SQL_C_SLONG:
                *reinterpret_cast<long*>(buffer) = jsValue->Int32Value();
                return sizeof(long);

            case SQL_C_DOUBLE:
                *reinterpret_cast<double*>(buffer) = jsValue->NumberValue();
                return sizeof(double);

            case SQL_C_BIT:
                *reinterpret_cast<bool*>(buffer) = jsValue->BooleanValue();
                return sizeof(bool);

            case SQL_C_TYPE_TIMESTAMP:
                if (!jsValue->IsDate())
                    return 0;

                {
                    long jsTime = static_cast<long>(jsValue.As<Date>()->NumberValue());
                    time_t time = jsTime;

                    tm tm = *gmtime(&time);
                    auto ts = reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(buffer);
                    ts->year = tm.tm_year + 1900;
                    ts->month = tm.tm_mon + 1;
                    ts->day = tm.tm_mday;
                    ts->hour = tm.tm_hour;
                    ts->minute = tm.tm_min;
                    ts->second = tm.tm_sec;
                    ts->fraction = (jsTime % 1000) * 100;
                    return sizeof(*ts);
                }

            case SQL_C_CHAR:
                {
                    String::Utf8Value val(jsValue);
                    if (!*val)
                        return false;

                    auto chars = min<size_t>(val.length(), length);
                    strncpy(reinterpret_cast<char*>(buffer), *val, chars); 
                    return chars;
                }

            case SQL_C_WCHAR:
                {
                    WStringValue val(jsValue);
                    if (!*val)
                        return false;

                    auto chars = min<size_t>(val.length(), length / sizeof(**val));
		    memcpy(buffer, *val, chars * sizeof(SQLWCHAR));
                    return chars * sizeof(**val);
                }

            default: 
                return 0;
            }
        }

        bool AllocateBoundInputParameter(SQLSMALLINT cType, Handle<Value> jsValue, SQLPOINTER& buffer, SQLLEN& length, Handle<Object>& handle) {
            switch (cType) {
            case SQL_C_SLONG:
                if(!AllocatePrimitive<long>(jsValue->Int32Value(), buffer, handle))
                    return false;
                length = sizeof(long);
                return true;

            case SQL_C_DOUBLE:
                if(!AllocatePrimitive<double>(jsValue->NumberValue(), buffer, handle))
                    return false;
                length = sizeof(double);
                return true;

            case SQL_C_BIT:
                if(!AllocatePrimitive<bool>(jsValue->BooleanValue(), buffer, handle))
                    return false;
                length = sizeof(bool);
                return true;

            case SQL_C_TYPE_TIMESTAMP:
                if (jsValue->IsDate()) {
                    long jsTime = static_cast<long>(jsValue.As<Date>()->NumberValue());
                    time_t time = jsTime;

                    tm tm = *gmtime(&time);
                    SQL_TIMESTAMP_STRUCT ts;
                    ts.year = tm.tm_year + 1900;
                    ts.month = tm.tm_mon + 1;
                    ts.day = tm.tm_mday;
                    ts.hour = tm.tm_hour;
                    ts.minute = tm.tm_min;
                    ts.second = tm.tm_sec;
                    ts.fraction = (jsTime % 1000) * 100;

                    if (!AllocatePrimitive<SQL_TIMESTAMP_STRUCT>(ts, buffer, handle))
                        return false;
                } else {
                    return false;
                }
                return true;

            case SQL_C_CHAR:
                if (jsValue->IsString()) {
                    handle = JSBuffer::New(jsValue.As<String>(), NanNew<String>("utf8"));
                    if (handle.IsEmpty())
                        return false;
                    JSBuffer::Unwrap(handle, buffer, length);
                } else {
                    return false; 
                }
                return true;

            case SQL_C_WCHAR:
                if (jsValue->IsString()) {
                    handle = JSBuffer::New(jsValue.As<String>(), NanNew<String>("ucs2"));
                    if (handle.IsEmpty())
                        return false;
                    JSBuffer::Unwrap(handle, buffer, length);
                } else {
                    return false; 
                }
                return true;

            case SQL_C_BINARY:
                if (Buffer::HasInstance(jsValue) 
                    || (jsValue->IsObject() 
                        && JSBuffer::Constructor()->StrictEquals(jsValue.As<Object>()->GetConstructor()))) 
                {
                    handle = jsValue.As<Object>();
                    if(JSBuffer::Unwrap(handle, buffer, length))
                        return false;
                }
                return true;

            default:
                return false;
            }
        }

        bool AllocateOutputBuffer(SQLSMALLINT cType, SQLPOINTER& buffer, SQLLEN& length, Handle<Object>& handle) {
            length = GetDesiredBufferLength(cType);
            if (length <= 0)
                return false;

            handle = JSBuffer::New(length);
            if(auto msg = JSBuffer::Unwrap(handle, buffer, length)) {
                EOS_DEBUG(L"%hs\n", msg);
                return false;
            }

            return true;
        }
    }
}
