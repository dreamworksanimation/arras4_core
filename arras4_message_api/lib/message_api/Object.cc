// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Object.h"

#include <sstream>
#include <string.h>
#include <iomanip>

namespace {
    
// For stringToValue : unfortunately unformatted conversion is not exposed in the 
// jsoncpp 0.6.0 API. This is similar to what is implemented in the source.

bool containsControlCharacter( const char* str )
{
   while ( *str ) 
   {
       char ch = *(str++);
       if (ch > 0 && ch <= 0x1F)
           return true;
   }
   return false;
}

void appendQuotedString(const char *value,std::stringstream& out)
{
    out << "\"";
    if (strpbrk(value, "\"\\\b\f\n\r\t") == NULL && !containsControlCharacter( value ))
    {
        out << value << "\"";
        return;
    }
 
   for (const char* c=value; *c != 0; ++c)
   {
      switch(*c)
      {
      case '\"':
          out << "\\\"";
          break;
      case '\\':
          out << "\\\\";
          break;
      case '\b':
          out << "\\b";
          break;
      case '\f':
          out << "\\f";
          break;
      case '\n':
          out << "\\n";
          break;
      case '\r':
          out << "\\r";
          break;
      case '\t':
          out << "\\t";
          break;
      default:
          if (*c > 0 && *c <= 0x1F) {
              std::stringstream tmp;
              tmp << "\\u" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << static_cast<int>(*c);
              out << tmp.str();
          }
          else
              out << *c;
          break;
      }
   }
   out << "\"";
}

void appendValue(const Json::Value& value,
                 bool quoteStrings,
                 std::stringstream& out)
{
    switch ( value.type() )
    {
    case Json::nullValue:
        out << "null";
        break;
    case Json::intValue:
        out << value.asLargestInt();
        break;
    case Json::uintValue:
        out << value.asLargestUInt();
        break;
    case Json::realValue:
        out << value.asDouble();
        break;
    case Json::stringValue:
        if (quoteStrings)
            appendQuotedString(value.asCString(),out);
        else
            out << value.asString();
        break;
    case Json::booleanValue:
        out << (value.asBool() ? "true":"false");
        break;
    case Json::arrayValue:
    {
        out << "[";
        int size = value.size();
        for ( int index =0; index < size; ++index )
        {
            if ( index > 0 )
                out << ",";
            appendValue( value[index],true,out );
        }
        out << "]";
    }
    break;
    case Json::objectValue:
    {
        Json::Value::Members members( value.getMemberNames() );
        out << "{";
        for ( Json::Value::Members::iterator it = members.begin(); 
              it != members.end(); 
              ++it )
        {
            const std::string &name = *it;
            if ( it != members.begin() )
                out << ",";
            appendQuotedString(name.c_str(),out);
            out << ":";
            appendValue(value[name],true,out);
         }
        out <<  "}";
    }
    break;
    }
}

} // namespace {

namespace arras4 {
    namespace api {

std::string valueToString(ObjectConstRef o,bool quoteStrings)
{
    std::stringstream ss;
    appendValue(o,quoteStrings,ss);
    return ss.str();
}

}
}
