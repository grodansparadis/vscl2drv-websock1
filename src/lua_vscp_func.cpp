// duktape_vscp.c
//
// This file is part of the VSCP (https://www.vscp.org)
//
// The MIT License (MIT)
//
// Copyright © 2000-2021 Ake Hedman, the VSCP project
// <akhe@vscp.org>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// wxJSON - http://wxcode.sourceforge.net/docs/wxjson/wxjson_tutorial.html
//

#include <list>
#include <string>

#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <json.hpp> // Needs C++11  -std=c++11

#include <actioncodes.h>
#include <controlobject.h>
#include <userlist.h>
#include <version.h>
#include <vscp.h>
#include <vscpdb.h>
#include <vscphelper.h>
#include <vscpremotetcpif.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define LUA_LIB
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#ifdef __cplusplus
}
#endif /* __cplusplus */

#include "lua_vscp_func.h"

// https://github.com/nlohmann/json
using json = nlohmann::json;

///////////////////////////////////////////////////
//                   GLOBALS
///////////////////////////////////////////////////

extern CControlObject* gpobj;

///////////////////////////////////////////////////////////////////////////////
// web_lsp_md5
//
// mg.md5
//

int
web_lsp_md5(lua_State* L)
{
    // int num_args = lua_gettop(L);
    // const char *text;
    // md5_byte_t hash[16];
    // md5_state_t ctx;
    // size_t text_len;
    // char buf[40];

    // if ( 1 == num_args ) {

    //     text = lua_tolstring (L, 1, &text_len );
    //     if ( text ) {
    //         vscpmd5_init( &ctx );
    //         vscpmd5_append( &ctx, (const md5_byte_t *) text, text_len );
    //         vscpmd5_finish( &ctx, hash);
    //         vscp_byteArray2HexStr( buf, hash, sizeof (hash) );
    //         lua_pushstring( L, buf );
    //     }
    //     else {
    //         lua_pushnil(L);
    //     }
    // }
    // else {
    //     // Syntax error
    //     return luaL_error(L, "invalid md5() call");
    // }
    return 1;
}

///////////////////////////////////////////////////
//                  HELPERS
///////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// lua_get_Event
//
// Make event from JSON data object on stack
//

int
lua_get_Event(struct lua_State* L, vscpEventEx* pex)
{
    char buf[1024];
    int len = 1;

    lua_pushlstring(L, buf, len);

    /*struct web_connection *conn =
            (struct web_connection *) lua_touserdata(L, lua_upvalueindex(1));
    int num_args = lua_gettop(L);

    // This function may be called with one parameter (boolean) to set the
    // keep_alive state.
    // Or without a parameter to just query the current keep_alive state.
    if ((num_args == 1) && lua_isboolean(L, 1)) {
        conn->must_close = !lua_toboolean(L, 1);
    }
    else if (num_args != 0) {
        // Syntax error
        return luaL_error(L, "invalid keep_alive() call");
    }*/

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_print
//

int
lua_vscp_print(struct lua_State* L)
{
    size_t len;
    const char* pstr;

    int nArgs = lua_gettop(L);

    if (0 == nArgs) {
        return luaL_error(L,
                          "vscp.print: Wrong number of arguments: "
                          "vscp.print(\"message\") ");
    }

    for (int i = 1; i < nArgs + 1; i++) {
        pstr = lua_tolstring(L, i, &len);
        if (NULL != pstr) {
            // xxPrintf( "%s\n", pstr );
        }
    }

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_log
//
// vscp.log("message"[,log-level,log-type])

int
lua_vscp_log(struct lua_State* L)
{
    std::string msg;
    uint8_t type = LOG_ERR;
    uint8_t __attribute__((unused)) level = 0;

    int nArgs = lua_gettop(L);

    if (0 == nArgs) {
        return luaL_error(L,
                          "vscp.log: Wrong number of arguments: "
                          "vscp.log(\"message\"[,log-level,log-type]) ");
    }

    if (nArgs >= 1) {
        if (!lua_isstring(L, 1)) {
            return luaL_error(L,
                              "vscp.log: Argument error, string expected: "
                              "vscp.log(\"message\"[,log-level,log-type]) ");
        }
        size_t len;
        const char* pstr = lua_tolstring(L, 1, &len);
        msg = std::string(pstr, len);
    }

    if (nArgs >= 2) {

        if (!lua_isnumber(L, 2)) {
            return luaL_error(L,
                              "vscp.log: Argument error, integer expected: "
                              "vscp.log(\"message\"[,log-level,log-type]) ");
        }

        level = lua_tointeger(L, 2);
    }

    if (nArgs >= 3) {

        if (!lua_isnumber(L, 3)) {
            return luaL_error(L,
                              "vscp.log: Argument error, integer expected: "
                              "vscp.log(\"message\"[,log-level,log-type]) ");
        }

        type = lua_tointeger(L, 3);
    }

    syslog(type, "%s", msg.c_str());

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_sleep
//
// vscp.sleep( milliseconds )

int
lua_vscp_sleep(struct lua_State* L)
{
    uint32_t sleep_ms;
    int nArgs = lua_gettop(L);

    if (0 == nArgs) {
        return luaL_error(L,
                          "vscp.sleep: Wrong number of arguments: "
                          "vscp.sleep( milliseconds ) ");
    } else {
        if (!lua_isnumber(L, 1)) {
            return luaL_error(L,
                              "vscp.sleep: Argument error, integer expected: "
                              "vscp.sleep( milliseconds ) ");
        }

        sleep_ms = (uint32_t)lua_tonumber(L, 1);
    }

    usleep(sleep_ms * 1000);

    lua_pushboolean(L, 1);
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_base64_encode
//
// vscp.base64encode(string )

int
lua_vscp_base64_encode(struct lua_State* L)
{
    std::string str;
    int nArgs = lua_gettop(L);

    if (0 == nArgs) {
        return luaL_error(L,
                          "vscp.base64encode: Wrong number of arguments: "
                          "vscp.base64encode( string_to_encode ) ");
    }

    // variable name
    if (!lua_isstring(L, 1)) {
        return luaL_error(L,
                          "vscp.base64encode: Argument error, string expected: "
                          "vscp.base64encode( string_to_encode ) ");
    }

    size_t len;
    const char* pstr = lua_tolstring(L, 1, &len);
    str = std::string(pstr, len);

    if (!vscp_base64_std_encode(str)) {
        return luaL_error(L, "vscp.base64encode: Failed to encode string!");
    }

    lua_pushlstring(L, (const char*)str.c_str(), str.length());

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_base64_decode
//
// vscp.base64decode(string )

int
lua_vscp_base64_decode(struct lua_State* L)
{
    std::string str;
    int nArgs = lua_gettop(L);

    if (0 == nArgs) {
        return luaL_error(L,
                          "vscp.base64decode: Wrong number of arguments: "
                          "vscp.base64decode( string_to_encode ) ");
    }

    // variable name
    if (!lua_isstring(L, 1)) {
        return luaL_error(L,
                          "vscp.base64decode: Argument error, string expected: "
                          "vscp.base64decode( string_to_encode ) ");
    }

    size_t len;
    const char* pstr = lua_tolstring(L, 1, &len);
    str = std::string(pstr, len);

    if (!vscp_base64_std_decode(str)) {
        return luaL_error(L, "vscp.base64decode: Failed to decode string!");
    }

    lua_pushlstring(L, (const char*)str.c_str(), str.length());

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_escapexml
//
// result = escapexml(string)
//
// TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO

int
lua_vscp_escapexml(struct lua_State* L)
{
    int nArgs = lua_gettop(L);

    if (0 == nArgs) {
        return luaL_error(L,
                          "vscp.escapexml: Wrong number of arguments: "
                          "vscp.escapexml( string-to-escape ) ");
    }

    // String to escape
    if (!lua_isstring(L, 1)) {
        return luaL_error(L,
                          "vscp.readvariable: Argument error, string expected: "
                          "vscp.readvariable( \"name\"[,format] ) ");
    }

    size_t len;
    const char* pstr = lua_tolstring(L, 1, &len);

    if (!len) {
        lua_pushnil(L);
        return 1;
    }

    // TODO
    pstr = pstr;

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_readVariable
//
// vscp.readvariable( "name"[, format ] )
//
//      format = 0 - string
//      format = 1 - XML
//      format = 2 - JSON
//      format = 3 - Just value (can be base64 encoded)
//      format = 4 - Just note (always base64 encoded)

int
lua_vscp_readVariable(struct lua_State* L)
{
    // int format = 0;
    // std::string varName;
    // CVariable variable;
    // std::string strResult;

    // int nArgs = lua_gettop( L );

    // if ( 0 == nArgs) {
    //     return luaL_error( L, "vscp.readvariable: Wrong number of arguments:
    //     "
    //                           "vscp.readvariable( \"name\"[,format] ) ");
    // }
    // else if ( 1 == nArgs ) {

    //     // variable name
    //     if ( !lua_isstring( L, 1 ) ) {
    //         return luaL_error( L, "vscp.readvariable: Argument error, string
    //         expected: "
    //                               "vscp.readvariable( \"name\"[,format] ) ");
    //     }

    //     size_t len;
    //     const char *pstr = lua_tolstring ( L, 1, &len );
    //     varName = std::string( pstr, len );

    // }
    // else {

    //     // variable name
    //     if ( !lua_isstring( L, 1 ) ) {
    //         return luaL_error( L, "vscp.readvariable: Argument error, string
    //         expected: "
    //                               "vscp.readvariable( \"name\"[,format] ) ");
    //     }

    //     size_t len;
    //     const char *pstr = lua_tolstring ( L, 1, &len );
    //     varName = std::string( pstr, len );

    //     // format
    //     if ( !lua_isnumber( L, 2 ) ) {
    //         return luaL_error( L, "vscp.readvariable: Argument error, number
    //         expected: "
    //                               "vscp.readvariable( \"name\"[,format] ) ");
    //     }

    //     format = (int)lua_tointeger( L, 2 );
    // }

    // CUserItem *pAdminUser = gpobj->m_userList.getUser(USER_ID_ADMIN);
    // if ( !gpobj->m_variables.find( varName, pAdminUser, variable ) ) {
    //     return luaL_error( L, "vscp.readvariable: No variable with that "
    //                           "name found!");
    // }

    // // Get the variable in string format
    // if ( 0 == format ) {
    //     // Get the variable in string format
    //     std::string varStr;
    //     varStr = variable.getAsString( false );
    //     lua_pushlstring( L, (const char *)varStr.c_str(),
    //                         varStr.length() );
    // }
    // // Get variable in XML format
    // else if ( 1 == format ) {
    //     // Get the variable in XML format
    //     std::string varXML;
    //     variable.getAsXML( varXML );
    //     lua_pushlstring( L, (const char *)varXML.c_str(),
    //                         varXML.length() );
    // }
    // // Get variable in JSON format
    // else if ( 2 == format ) {
    //     // Get the variable on JSON format
    //     std::string varJSON;
    //     variable.getAsJSON( varJSON );
    //     lua_pushlstring( L, (const char *)varJSON.c_str(),
    //                         varJSON.length() );
    // }
    // // Get only value
    // else if ( 3 == format ) {
    //     // Get the variable value
    //     std::string strval = variable.getValue();
    //     if ( variable.isNumerical() ) {
    //         if ( VSCP_DAEMON_VARIABLE_CODE_INTEGER == variable.getType() ) {
    //             int val = atoi( strval.c_str() );
    //             lua_pushinteger( L, val );
    //         }
    //         else if ( VSCP_DAEMON_VARIABLE_CODE_LONG == variable.getType() )
    //         {
    //             long val = atol( strval.c_str() );
    //             lua_pushnumber( L, val );
    //         }
    //         else if ( VSCP_DAEMON_VARIABLE_CODE_DOUBLE == variable.getType()
    //         ) {
    //             double val;
    //             val = std::stod(strval);
    //             lua_pushnumber( L, val );
    //         }
    //     }
    //     else {
    //         if ( VSCP_DAEMON_VARIABLE_CODE_BOOLEAN == variable.getType() ) {
    //             lua_pushboolean( L, variable.isTrue() );
    //         }
    //         else {

    //             lua_pushlstring( L, (const char *)strval.c_str(),
    //                                               strval.length() );
    //         }
    //     }
    // }
    // // Get only note
    // else if ( 4 == format ) {
    //     // Get the variable value un encoded
    //     std::string strval = variable.getValue();
    //     lua_pushlstring( L, (const char *)strval.c_str(),
    //                                       strval.length() );
    // }
    // else {
    //     return luaL_error( L, "vscp.readvariable: Format must be 0=string, "
    //                           "1=XML, 2=JSON, 3=value, 4=note");
    // }

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_writeVariable
//
// writeVariable( value[,format])
//
//      format = 0 - string
//      format = 1 - XML
//      format = 2 - JSON
//

int
lua_vscp_writeVariable(struct lua_State* L)
{
    // size_t len;
    // int format = 0;
    // bool bBase64 = false;
    // std::string varValue;
    // CVariable variable;

    // int nArgs = lua_gettop( L );

    // if ( 0 == nArgs ) {
    //     return luaL_error( L, "vscp.writeVariable: Wrong number of arguments:
    //     "
    //                           "vscp.writeVariable( varname[,format] ) ");
    // }

    // if ( nArgs >= 1 ) {

    //     // variable value
    //     if ( !lua_isstring( L, 1 ) ) {
    //         return luaL_error( L, "vscp.writeVariable: Argument error, string
    //         expected: "
    //                               "vscp.writeVariable( name[,format] ) ");
    //     }

    //     const char *pstr = lua_tolstring ( L, 1, &len );
    //     varValue = std::string( pstr, len );

    // }

    // if ( nArgs >= 2 ) {

    //     // format
    //     if ( !lua_isnumber( L, 2 ) ) {
    //         return luaL_error( L, "vscp.writevariable: Argument error, number
    //         expected: "
    //                               "vscp.writevariable( name[,format] ) ");
    //     }

    //     format = (int)lua_tointeger( L, 2 );
    // }

    // if ( 0 == format ) {
    //     // Set variable from string
    //     if ( !variable.setFromString( varValue ) ) {
    //         return luaL_error( L, "vscp.writevariable: Could not set
    //         variable"
    //                               " from string.");
    //     }
    // }
    // else if ( 1 == format ) {
    //     // Set variable from XML object
    //     if ( !variable.setFromXML( varValue ) ) {
    //         return luaL_error( L, "vscp.writevariable: Could not set
    //         variable"
    //                               " from XML object.");
    //     }
    // }
    // else if ( 2 == format ) {
    //     // Set variable from JSON object
    //     if ( !variable.setFromJSON( varValue ) ) {
    //         return luaL_error( L, "vscp.writevariable: Could not set
    //         variable"
    //                               " from JSON object.");
    //     }
    // }
    // else {
    //     return luaL_error( L, "vscp.writevariable: Format must be 0=string, "
    //                           "1=XML, 2=JSON");
    // }

    // // Update or add variable if not defined
    // if ( !gpobj->m_variables.add( variable ) ) {
    //     return luaL_error( L, "vscp.writevariable: Failed to update/add
    //     variable!");
    // }

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_writeVariableValue
//
// writeVariableValue( varname, value[, bBase64])
//
//

int
lua_vscp_writeVariableValue(struct lua_State* L)
{
    // size_t len;
    // const char *pstr;
    // bool bBase64 = false;
    // std::string varName;
    // std::string varValue;
    // CVariable variable;

    // int nArgs = lua_gettop( L );

    // if ( nArgs < 2 ) {
    //     return luaL_error( L, "vscp.writeVariableValue: Wrong number of
    //     arguments: "
    //                           "vscp.writeVariableValue( varname, varvalue[,
    //                           bBase64] ) ");
    // }

    // // variable name
    // if ( !lua_isstring( L, 1 ) ) {
    //     return luaL_error( L, "vscp.writeVariableValue: Argument error,
    //     string expected: "
    //                           "vscp.writeVariableValue( varname, varvalue,
    //                           bBase64 ) ");
    // }

    // pstr = lua_tolstring ( L, 1, &len );
    // varName = std::string( pstr, len );

    // CUserItem *pAdminUser = gpobj->m_userList.getUser(USER_ID_ADMIN);
    // if ( !gpobj->m_variables.find( varName, pAdminUser, variable ) ) {
    //     return luaL_error( L, "vscp.writeVariableValue: No variable with that
    //     "
    //                           "name found!");
    // }

    // // variable value
    // if ( variable.isNumerical() ) {

    //     if ( VSCP_DAEMON_VARIABLE_CODE_INTEGER == variable.getType() ) {

    //         int val;
    //         if ( lua_isnumber( L, 2 ) ) {
    //             val = lua_tointeger( L, 2 );
    //             variable.setValue( val );
    //         }
    //         else if ( lua_isstring( L, 2 ) ) {
    //             pstr = lua_tolstring ( L, 2, &len );
    //             val = atoi( pstr );
    //             variable.setValue( val );
    //         }
    //         else {
    //             return luaL_error( L, "vscp.writeVariableValue: Value has "
    //                                   "wrong format !");
    //         }

    //     }
    //     else if ( VSCP_DAEMON_VARIABLE_CODE_LONG == variable.getType() ) {

    //         long val;
    //         if ( lua_isnumber( L, 2 ) ) {
    //             val = lua_tonumber( L, 2 );
    //             variable.setValue( val );
    //         }
    //         else if ( lua_isstring( L, 2 ) ) {
    //             pstr = lua_tolstring ( L, 2, &len );
    //             val = atol( pstr );
    //             variable.setValue( val );
    //         }

    //         else {
    //             return luaL_error( L, "vscp.writeVariableValue: Value has "
    //                                   "wrong format !");
    //         }

    //     }
    //     else if ( VSCP_DAEMON_VARIABLE_CODE_DOUBLE == variable.getType() ) {

    //         double val;
    //         if ( lua_isnumber( L, 2 ) ) {
    //             val = lua_tonumber( L, 2 );
    //             variable.setValue( val );
    //         }
    //         else if ( lua_isstring( L, 2 ) ) {
    //             val = lua_tonumber( L, 2 );
    //             variable.setValue( val );
    //         }

    //         else {
    //             return luaL_error( L, "vscp.writeVariableValue: Value has "
    //                                   "wrong format !");
    //         }

    //     }

    // }
    // else {

    //     if ( VSCP_DAEMON_VARIABLE_CODE_BOOLEAN == variable.getType() ) {
    //         if ( lua_isboolean( L, 2 ) ) {
    //             bool bVal   = lua_toboolean( L, 2 ) ? true : false;
    //             variable.setValue( bVal );
    //         }
    //         else {
    //             return luaL_error( L, "vscp.writeVariableValue: Value has "
    //                                   "wrong format !");
    //         }
    //     }
    //     else {
    //         // This is a string type value

    //         // variable base64 flag
    //         if ( nArgs < 3 ) {
    //             bBase64 = lua_toboolean ( L, 3 );
    //         }

    //         pstr = lua_tolstring ( L, 1, &len );
    //         std::string str = std::string( pstr, len );
    //         variable.setValue( str, bBase64 );
    //     }

    // }

    // if ( !gpobj->m_variables.update( variable, pAdminUser ) ) {
    //     return luaL_error( L, "vscp.writeVariableValue: Failed to update
    //     variable value!");
    // }

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_writeVariableNote
//
// writeVariableNote( varname, note, bBase64 )
//
//

int
lua_vscp_writeVariableNote(struct lua_State* L)
{
    // std::string varName;
    // std::string varValue;
    // CVariable variable;

    // int nArgs = lua_gettop( L );

    // if ( nArgs < 3 ) {
    //     return luaL_error( L, "vscp.writeVariableNote: Wrong number of
    //     arguments: "
    //                           "vscp.writeVariableNote( varname, varvalue,
    //                           bBase64 ) ");
    // }

    // // variable name
    // if ( !lua_isstring( L, 1 ) ) {
    //     return luaL_error( L, "vscp.writeVariableNote: Argument error, string
    //     expected: "
    //                           "vscp.writeVariableNote( varname, varvalue,
    //                           bBase64 ) ");
    // }

    // size_t len;
    // const char *pstr = lua_tolstring ( L, 1, &len );
    // varName = std::string( pstr, len );

    // // variable value
    // if ( !lua_isstring( L, 2 ) ) {
    //     return luaL_error( L, "vscp.writeVariableNote: Argument error, string
    //     expected: "
    //                           "vscp.writeVariableNote( varname, varvalue,
    //                           bBase64 ) ");
    // }

    // pstr = lua_tolstring ( L, 2, &len );
    // varValue = std::string( pstr, len );

    // // variable base64 flag
    // bool bBase64 = false;
    // if ( !lua_isboolean( L, 3 ) ) {
    //     return luaL_error( L, "vscp.writeVariableNote: Argument error,
    //     boolean expected: "
    //                           "vscp.writeVariableNote( varname, varvalue,
    //                           bBase64 ) ");
    // }

    // bBase64 = lua_toboolean ( L, 3 );

    // CUserItem *pAdminUser = gpobj->m_userList.getUser(USER_ID_ADMIN);
    // if ( !gpobj->m_variables.find( varName, pAdminUser, variable ) ) {
    //     return luaL_error( L, "vscp.writeVariableNote: No variable with that
    //     "
    //                           "name found!");
    // }

    // variable.setNote( varValue, bBase64 );

    // if ( !gpobj->m_variables.update( variable, pAdminUser ) ) {
    //     return luaL_error( L, "vscp.writeVariableNote: Failed to update
    //     variable note!");
    // }

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_deleteVariable
//

int
lua_vscp_deleteVariable(struct lua_State* L)
{
    // std::string varName;
    // CVariable variable;

    // int nArgs = lua_gettop( L );

    // if ( 0 == nArgs  ) {
    //     return luaL_error( L, "vscp.deleteVariable: Wrong number of
    //     arguments: "
    //                           "vscp.deleteVariable( varname ) ");
    // }

    // // variable name
    // if ( !lua_isstring( L, 1 ) ) {
    //     return luaL_error( L, "vscp.deleteVariable: Argument error, string
    //     expected: "
    //                           "vscp.deleteVariable( varname  ) ");
    // }

    // size_t len;
    // const char *pstr = lua_tolstring ( L, 1, &len );
    // varName = std::string( pstr, len );

    // CUserItem *pAdminUser = gpobj->m_userList.getUser(USER_ID_ADMIN);
    // if ( !gpobj->m_variables.remove( varName, pAdminUser ) ) {
    //     return luaL_error( L, "vscp.deleteVariable: Failed to delete
    //     variable!");
    // }

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_isVariableBase64Encoded
//
// isVariableBase64(name)
//
//

int
lua_vscp_isVariableBase64Encoded(struct lua_State* L)
{
    // size_t len;
    // bool bBase64 = false;
    // std::string varName;
    // CVariable variable;

    // int nArgs = lua_gettop( L );

    // if ( 0 == nArgs ) {
    //     return luaL_error( L, "vscp.isVariableBase64: Wrong number of
    //     arguments: "
    //                           "vscp.isVariableBase64( name ) ");
    // }

    // if ( nArgs >= 1 ) {

    //     // variable name
    //     if ( !lua_isstring( L, 1 ) ) {
    //         return luaL_error( L, "vscp.isVariableBase64: Argument error,
    //         string expected: "
    //                               "vscp.isVariableBase64( name ) ");
    //     }

    //     const char *pstr = lua_tolstring ( L, 1, &len );
    //     varName = std::string( pstr, len );

    // }

    // CUserItem *pAdminUser = gpobj->m_userList.getUser(USER_ID_ADMIN);
    // if ( !gpobj->m_variables.find( varName, pAdminUser, variable ) ) {
    //     return luaL_error( L, "vscp.isVariableBase64: No variable with that "
    //                           "name found!");
    // }

    // lua_pushboolean( L, false );

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_isVariablePersistent
//
// isVariablePersistent(name)
//
//

int
lua_vscp_isVariablePersistent(struct lua_State* L)
{
    // size_t len;
    // bool bBase64 = false;
    // std::string varName;
    // CVariable variable;

    // int nArgs = lua_gettop( L );

    // if ( 0 == nArgs ) {
    //     return luaL_error( L, "vscp.isVariablePersistent: Wrong number of
    //     arguments: "
    //                           "vscp.isVariablePersistent( name ) ");
    // }

    // if ( nArgs >= 1 ) {

    //     // variable name
    //     if ( !lua_isstring( L, 1 ) ) {
    //         return luaL_error( L, "vscp.isVariablePersistent: Argument error,
    //         string expected: "
    //                               "vscp.isVariablePersistent( name ) ");
    //     }

    //     const char *pstr = lua_tolstring ( L, 1, &len );
    //     varName = std::string( pstr, len );

    // }

    // CUserItem *pAdminUser = gpobj->m_userList.getUser(USER_ID_ADMIN);
    // if ( !gpobj->m_variables.find( varName, pAdminUser, variable ) ) {
    //     return luaL_error( L, "vscp.isVariablePersistent: No variable with
    //     that "
    //                           "name found!");
    // }

    // lua_pushboolean( L, variable.isPersistent() );

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_isVariableNumerical
//
// isVariableNumerical(name)
//
//

int
lua_vscp_isVariableNumerical(struct lua_State* L)
{
    // size_t len;
    // bool bBase64 = false;
    // std::string varName;
    // CVariable variable;

    // int nArgs = lua_gettop( L );

    // if ( 0 == nArgs ) {
    //     return luaL_error( L, "vscp.isVariableNumerical: Wrong number of
    //     arguments: "
    //                           "vscp.isVariableNumerical( name ) ");
    // }

    // if ( nArgs >= 1 ) {

    //     // variable name
    //     if ( !lua_isstring( L, 1 ) ) {
    //         return luaL_error( L, "vscp.isVariableNumerical: Argument error,
    //         string expected: "
    //                               "vscp.isVariableNumerical( name ) ");
    //     }

    //     const char *pstr = lua_tolstring ( L, 1, &len );
    //     varName = std::string( pstr, len );

    // }

    // CUserItem *pAdminUser = gpobj->m_userList.getUser(USER_ID_ADMIN);
    // if ( !gpobj->m_variables.find( varName, pAdminUser, variable ) ) {
    //     return luaL_error( L, "vscp.isVariableNumerical: No variable with
    //     that "
    //                           "name found!");
    // }

    // lua_pushboolean( L, variable.isNumerical() );

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_isStockVariable
//
// isStockVariable(name)
//
//

int
lua_vscp_isStockVariable(struct lua_State* L)
{
    // size_t len;
    // bool bBase64 = false;
    // std::string varName;
    // CVariable variable;

    // int nArgs = lua_gettop( L );

    // if ( 0 == nArgs ) {
    //     return luaL_error( L, "vscp.isStockVariable: Wrong number of
    //     arguments: "
    //                           "vscp.isStockVariable( name ) ");
    // }

    // if ( nArgs >= 1 ) {

    //     // variable name
    //     if ( !lua_isstring( L, 1 ) ) {
    //         return luaL_error( L, "vscp.isStockVariable: Argument error,
    //         string expected: "
    //                               "vscp.isStockVariable( name ) ");
    //     }

    //     const char *pstr = lua_tolstring ( L, 1, &len );
    //     varName = std::string( pstr, len );

    // }

    // CUserItem *pAdminUser = gpobj->m_userList.getUser(USER_ID_ADMIN);
    // if ( !gpobj->m_variables.find( varName, pAdminUser, variable ) ) {
    //     return luaL_error( L, "vscp.isStockVariable: No variable with that "
    //                           "name found!");
    // }

    // lua_pushboolean( L, variable.isStockVariable() );

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_sendEvent
//
// lua_vscp_sendEvent( event[,format] )
//
//      format = 0 - String format.
//      format = 1 - XML format.
//      format = 2 - JSON format.
//

int
lua_vscp_sendEvent(struct lua_State* L)
{
    int format = 0;
    vscpEventEx ex;
    std::string strEvent;
    CClientItem* pClientItem = NULL;

    int nArgs = lua_gettop(L);

    // Get the client item
    lua_pushlstring(L, "vscp_clientitem", 15);
    lua_gettable(L, LUA_REGISTRYINDEX);
    pClientItem = (CClientItem*)lua_touserdata(L, -1);

    nArgs = lua_gettop(L);

    // Remove client item from stack
    lua_pop(L, 1);

    nArgs = lua_gettop(L);

    if (NULL == pClientItem) {
        return luaL_error(L, "vscp.sendEvent: VSCP server client not found.");
    }

    if (nArgs < 1) {
        return luaL_error(L,
                          "vscp.sendEvent: Wrong number of arguments: "
                          "vscp.sendEvent( event[,format] ) ");
    }

    // Event
    if (!lua_isstring(L, 1)) {
        return luaL_error(L,
                          "vscp.sendEvent: Argument error, string expected: "
                          "vscp.sendEvent( event[,format] ) ");
    }

    size_t len;
    const char* pstr = lua_tolstring(L, 1, &len);
    strEvent = std::string(pstr, len);

    if (nArgs >= 2) {

        // format
        if (!lua_isnumber(L, 2)) {
            return luaL_error(
              L,
              "vscp.sendEvent: Argument error, number expected: "
              "vscp.sendEvent( event[,format] ) ");
        }

        format = (int)lua_tointeger(L, 2);
    }

    // String
    if (0 == format) {
        if (!vscp_convertStringToEventEx(&ex, strEvent)) {
            return luaL_error(
              L, "vscp.sendEvent: Failed to get VSCP event from string!");
        }
    }
    // XML
    else if (1 == format) {
        if (!vscp_convertXMLToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.sendEvent: Failed to get "
                              "VSCP event from XML!");
        }
    }
    // JSON
    else if (2 == format) {
        if (!vscp_convertJSONToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.sendEvent: Failed to get "
                              "VSCP event from JSON!");
        }
    }

    vscpEvent* pEvent = new vscpEvent;
    if (NULL == pEvent) {
        return luaL_error(L,
                          "vscp.sendEvent: Failed to  "
                          "allocate VSCP event!");
    }
    pEvent->pdata = NULL;
    vscp_convertEventExToEvent(pEvent, &ex);

    if (!gpobj->sendEvent(pClientItem, pEvent)) {
        // Failed to send event
        vscp_deleteEvent_v2(&pEvent);
        return luaL_error(L, "vscp.sendEvent: Failed to send event!");
    }

    vscp_deleteEvent_v2(&pEvent);

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_getEvent
//
// receiveevent([format])
//

int
lua_vscp_getEvent(struct lua_State* L)
{
    int format = 0;
    //vscpEventEx ex;
    std::string strEvent;
    CClientItem* pClientItem = NULL;

    int nArgs = lua_gettop(L);

    // Get format - if given
    if (0 != nArgs) {

        if (!lua_isnumber(L, 1)) {
            format = (int)lua_tointeger(L, 1);
        }
    }

    // Get the client item
    lua_pushlstring(L, "vscp_clientitem", 15);
    lua_gettable(L, LUA_REGISTRYINDEX);
    pClientItem = (CClientItem*)lua_touserdata(L, -1);

    if (NULL == pClientItem) {
        return luaL_error(L, "vscp.getEvent: VSCP server client not found.");
    }

//try_again:

    // Check the client queue
    if (pClientItem->m_bOpen && pClientItem->m_clientInputQueue.size()) {

        std::deque<vscpEvent*>::iterator it;
        vscpEvent* pEvent;

        pthread_mutex_lock(&pClientItem->m_mutexClientInputQueue);
        pEvent = pClientItem->m_clientInputQueue.front();
        pClientItem->m_clientInputQueue.pop_front();
        if (NULL == pEvent) {
            return luaL_error(L, "vscp.getEvent: Failed to get event.");
        }
        pthread_mutex_unlock(&pClientItem->m_mutexClientInputQueue);

        if (NULL == pEvent) {
            return luaL_error(L,
                              "vscp.getEvent: Allocation error when "
                              "getting event from client!");
        }

        if (vscp_doLevel2Filter(pEvent, &pClientItem->m_filter)) {

            // Write it out
            std::string strResult;
            switch (format) {
                case 0: // String
                    if (!vscp_convertEventToString(strResult, pEvent)) {
                        return luaL_error(L,
                                          "vscp.getEvent: Failed to "
                                          "convert event to string form.");
                    }
                    break;

                case 1: // XML
                    if (!vscp_convertEventToXML(strResult, pEvent)) {
                        return luaL_error(L,
                                          "vscp.getEvent: Failed to "
                                          "convert event to XML form.");
                    }
                    break;

                case 2: // JSON
                    if (!vscp_convertEventToJSON(strResult, pEvent)) {
                        return luaL_error(L,
                                          "vscp.getEvent: Failed to "
                                          "convert event to JSON form.");
                    }
                    break;
            }

            // Event is not needed anymore
            vscp_deleteEvent(pEvent);

            lua_pushlstring(
              L, (const char*)strResult.c_str(), strResult.length());

            // All OK return event
            return 1;

        } // Valid pEvent pointer

    } // events available

    // No events available
    lua_pushnil(L);
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_getCountEvent
//
// cnt = vscp.countevents()
//

int
lua_vscp_getCountEvent(struct lua_State* L)
{
    int count = 0;
    CClientItem* pClientItem = NULL;

    // Get the client item
    lua_pushlstring(L, "vscp_clientitem", 15);
    lua_gettable(L, LUA_REGISTRYINDEX);
    pClientItem = (CClientItem*)lua_touserdata(L, -1);

    if (NULL == pClientItem) {
        return luaL_error(L,
                          "vscp.getCountEvent: VSCP server client not found.");
    }

    if (pClientItem->m_bOpen) {
        count = pClientItem->m_clientInputQueue.size();
    } else {
        count = 0;
    }

    lua_pushinteger(L, count); // return count

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_vscp_setFilter
//
// setFilter( strFilter[, format] )
//
// format = 0 - From string
// format = 1 - From XML
// format = 2 - from JSON
// {
//     'mask_priorit': number,
//     'mask_class': number,
//     'mask_type': number,
//     'mask_guid': 'string',
//     'filter_priority'; number,
//     'filter_class': number,
//     'filter_type': number,
//     'filter_guid' 'string'
// }

int
lua_vscp_setFilter(struct lua_State* L)
{
    int format = 0;
    vscpEventFilter filter;
    std::string strFilter;
    CClientItem* pClientItem = NULL;

    int nArgs = lua_gettop(L);

    // Get the client item
    lua_pushlstring(L, "vscp_clientitem", 15);
    lua_gettable(L, LUA_REGISTRYINDEX);
    pClientItem = (CClientItem*)lua_touserdata(L, -1);

    if (NULL == pClientItem) {
        return luaL_error(L, "vscp.setFilter: VSCP server client not found.");
    }

    if (nArgs < 1) {
        return luaL_error(L,
                          "vscp.setFilter: Wrong number of arguments: "
                          "vscp.setFilter( filter[,format] ) ");
    }

    // Event
    if (!lua_isstring(L, 1)) {
        return luaL_error(L,
                          "vscp.setFilter: Argument error, string expected: "
                          "vscp.setFilter( filter[,format] ) ");
    }

    size_t len;
    const char* pstr = lua_tolstring(L, 1, &len);
    strFilter = std::string(pstr, len);

    if (nArgs >= 2) {

        // format
        if (!lua_isnumber(L, 2)) {
            return luaL_error(
              L,
              "vscp.setFilter: Argument error, number expected: "
              "vscp.setFilter( filter[,format] ) ");
        }

        format = (int)lua_tointeger(L, 2);
    }

    switch (format) {

        case 0:
            if (!vscp_readFilterMaskFromString(&filter, strFilter)) {
                luaL_error(L, "vscp.setFilter: Failed to read filter!");
            }
            break;

        case 1:
            if (!vscp_readFilterMaskFromXML(&filter, strFilter)) {
                luaL_error(L, "vscp.setFilter: Failed to read filter!");
            }
            break;

        case 2:
            if (!vscp_readFilterMaskFromJSON(&filter, strFilter)) {
                luaL_error(L, "vscp.setFilter: Failed to read filter!");
            }
            break;
    }

    // Set the filter
    vscp_copyVSCPFilter(&pClientItem->m_filter, &filter);

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_send_Measurement
//
//      level       integer     1 or 2
//      string      boolean     true or false for string format event
//      value       double
//      guid        string      defaults to
//      "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00" vscptype    integer
//      unit        integer     default is 0    optional
//      sensorindex integer     default is 0    optional
//      zone        integer     default is 0    optional
//      subzone     integer     default is 0    optional
//

int
lua_send_Measurement(struct lua_State* L)
{
    CClientItem* pClientItem = NULL;
    vscpEvent* pEvent;
    double value;         // Measurement value
    bool bLevel2 = true;  // True if level II
    bool bString = false; // If level II string or float
    int type;             // VSCP type
    int unit = 0;
    int sensoridx = 0;
    int zone = 0;
    int subzone = 0;
    uint8_t guid[16];

    int nArgs = lua_gettop(L);

    // Get the client item
    lua_pushlstring(L, "vscp_clientitem", 15);
    lua_gettable(L, LUA_REGISTRYINDEX);
    pClientItem = (CClientItem*)lua_touserdata(L, -1);

    if (NULL == pClientItem) {
        return luaL_error(
          L, "vscp.sendMeasurement: VSCP server client not found.");
    }

    // Must be at least four args
    if (nArgs < 5) {
        return luaL_error(
          L,
          "vscp.sendMeasurement: Wrong number of arguments: "
          "vscp.sendMeasurement( level, bString, value, GUID, type"
          "[unit,index,zone,subzone] ) ");
    }

    // level
    if (!lua_isinteger(L, 1)) {
        return luaL_error(
          L,
          "vscp.sendMeasurement: Argument error, integer expected: "
          "vscp.sendMeasurement( level, bString, value, GUID, type"
          "[unit,index,zone,subzone] ) ");
    }

    int level = (int)lua_tointeger(L, 1);
    if (1 == level)
        bLevel2 = false;

    // bString
    if (!lua_isboolean(L, 2)) {
        return luaL_error(
          L,
          "vscp.sendMeasurement: Argument error, boolean expected: "
          "vscp.sendMeasurement( level, bString, value, GUID, type"
          "[unit,index,zone,subzone] ) ");
    }

    bString = lua_toboolean(L, 2);

    // value
    if (!lua_isnumber(L, 3)) {
        return luaL_error(
          L,
          "vscp.sendMeasurement: Argument error, number expected: "
          "vscp.sendMeasurement( level, bString, value, GUID, type"
          "[unit,index,zone,subzone] ) ");
    }

    value = lua_tonumber(L, 3);

    // GUID
    if (!lua_isstring(L, 4)) {
        return luaL_error(
          L,
          "vscp.sendMeasurement: Argument error, string expected: "
          "vscp.sendMeasurement( level, bString, value, GUID, type"
          "[unit,index,zone,subzone] ) ");
    }

    size_t len;
    const char* pstr = lua_tolstring(L, 4, &len);
    std::string strGUID = std::string(pstr, len);
    if (!vscp_getGuidFromStringToArray(guid, strGUID)) {
        return luaL_error(L, "vscp.sendMeasurement: Invalid GUID!");
    }

    // vscp type
    if (!lua_isinteger(L, 5)) {
        return luaL_error(
          L,
          "vscp.sendMeasurement: Argument error, integer expected: "
          "vscp.sendMeasurement( level, bString, value, GUID, type"
          "[unit,index,zone,subzone] ) ");
    }

    type = (int)lua_tointeger(L, 5);

    if (nArgs >= 5) {

        // unit
        if (!lua_isinteger(L, 6)) {
            return luaL_error(
              L,
              "vscp.sendMeasurement: Argument error, integer expected: "
              "vscp.sendMeasurement( level, bString, value, GUID, type"
              "[unit,index,zone,subzone] ) ");
        }

        unit = (int)lua_tointeger(L, 6);
    }

    if (nArgs >= 7) {

        // zone
        if (!lua_isinteger(L, 7)) {
            return luaL_error(
              L,
              "vscp.sendMeasurement: Argument error, integer expected: "
              "vscp.sendMeasurement( level, bString, value, GUID, type"
              "[unit,index,zone,subzone] ) ");
        }

        zone = (int)lua_tointeger(L, 7);
    }

    if (nArgs >= 8) {

        // subzone
        if (!lua_isinteger(L, 8)) {
            return luaL_error(
              L,
              "vscp.sendMeasurement: Argument error, integer expected: "
              "vscp.sendMeasurement( level, bString, value, GUID, type"
              "[unit,index,zone,subzone] ) ");
        }

        subzone = (int)lua_tointeger(L, 8);
    }

    if (bLevel2) {

        if (bString) {

            pEvent = new vscpEvent;
            if (NULL == pEvent) {
                return luaL_error(L,
                                  "vscp.sendMeasurement: "
                                  "Event allocation error");
            }
            pEvent->pdata = NULL;

            // Set GUID
            memcpy(pEvent->GUID, guid, 16);

            if (!vscp_makeLevel2StringMeasurementEvent(
                  pEvent, type, value, unit, sensoridx, zone, subzone)) {
                // Failed
                return luaL_error(L,
                                  "vscp.sendMeasurement: Failed to send "
                                  "measurement event!");
            }

        } else {

            pEvent = new vscpEvent;
            if (NULL == pEvent) {
                return luaL_error(L,
                                  "vscp.sendMeasurement: "
                                  "Event allocation error");
            }
            pEvent->pdata = NULL;

            // Set GUID
            memcpy(pEvent->GUID, guid, 16);

            if (!vscp_makeLevel2FloatMeasurementEvent(
                  pEvent, type, value, unit, sensoridx, zone, subzone)) {
                // Failed
                return luaL_error(L,
                                  "vscp.sendMeasurement: Failed to construct "
                                  "float measurement event!");
            }
        }

    } else {

        // Level I

        if (bString) {

            pEvent = new vscpEvent;
            if (NULL == pEvent) {
                return luaL_error(L,
                                  "vscp.sendMeasurement: "
                                  "Event allocation error!");
            }

            memcpy(pEvent->GUID, guid, 16);
            pEvent->vscp_type = type;
            pEvent->vscp_class = VSCP_CLASS1_MEASUREMENT;
            pEvent->obid = 0;
            pEvent->timestamp = 0;
            pEvent->pdata = NULL;

            if (!vscp_makeStringMeasurementEvent(
                  pEvent, value, unit, sensoridx)) {
                vscp_deleteEvent(pEvent);
                return luaL_error(L,
                                  "vscp.sendMeasurement: "
                                  "String event conversion failed!");
            }

            //

        } else {

            pEvent = new vscpEvent;
            if (NULL == pEvent) {
                return luaL_error(L,
                                  "vscp.sendMeasurement: "
                                  "Event allocation error!");
            }

            memcpy(pEvent->GUID, guid, 16);
            pEvent->vscp_type = type;
            pEvent->vscp_class = VSCP_CLASS1_MEASUREMENT;
            pEvent->obid = 0;
            pEvent->timestamp = 0;
            pEvent->pdata = NULL;

            if (!vscp_makeFloatMeasurementEvent(
                  pEvent, value, unit, sensoridx)) {
                vscp_deleteEvent(pEvent);
                return luaL_error(L,
                                  "vscp.sendMeasurement: Failed to send "
                                  "measurement event!");
            }
        }
    }

    // Send the event
    if (!gpobj->sendEvent(pClientItem, pEvent)) {
        // Failed to send event
        vscp_deleteEvent_v2(&pEvent);
        return luaL_error(L,
                          "vscp.sendMeasurement: "
                          "Failed to send event!");
    }

    vscp_deleteEvent_v2(&pEvent);

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_is_Measurement
//
// result = ismeasurement(event,format)
//
//      format = 0 - String format.
//      format = 1 - XML format.
//      format = 2 - JSON format.
//

int
lua_is_Measurement(struct lua_State* L)
{
    vscpEventEx ex;
    int format = 0;
    std::string strEvent;

    int nArgs = lua_gettop(L);

    // Event
    if (!lua_isstring(L, 1)) {
        return luaL_error(L,
                          "vscp.isMeasurement: Argument error, "
                          "string expected: "
                          "vscp.isMeasurement( event[,format] ) ");
    }

    size_t len;
    const char* pstr = lua_tolstring(L, 1, &len);
    strEvent = std::string(pstr, len);

    if (nArgs >= 2) {

        // format
        if (!lua_isnumber(L, 2)) {
            return luaL_error(L,
                              "vscp.isMeasurement: Argument error, "
                              "number expected: "
                              "vscp.isMeasurement( event[,format] ) ");
        }

        format = (int)lua_tointeger(L, 2);
    }

    if (0 == format) {
        if (!vscp_convertStringToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.isMeasurement: Failed to get VSCP "
                              "event from string!");
        }
    } else if (1 == format) {
        if (!vscp_convertXMLToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.isMeasurement: Failed to get "
                              "VSCP event from XML!");
        }
    } else if (2 == format) {
        if (!vscp_convertJSONToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.isMeasurement: Failed to get "
                              "VSCP event from JSON!");
        }
    }

    vscpEvent* pEvent = new vscpEvent;
    if (NULL == pEvent) {
        return luaL_error(L, "vscp.isMeasurement: Allocation error!");
    }

    pEvent->pdata = NULL;

    vscp_convertEventExToEvent(pEvent, &ex);
    bool bMeasurement = vscp_isMeasurement(pEvent);
    vscp_deleteEvent(pEvent);

    lua_pushboolean(L, bMeasurement ? 1 : 0);
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_get_MeasurementValue
//

int
lua_get_MeasurementValue(struct lua_State* L)
{
    double value;
    vscpEventEx ex;
    int format = 0;
    std::string strEvent;

    int nArgs = lua_gettop(L);

    // Event
    if (!lua_isstring(L, 1)) {
        return luaL_error(L,
                          "vscp.getMeasurementValue: Argument error, "
                          "string expected: "
                          "vscp.getMeasurementValue( event[,format] ) ");
    }

    size_t len;
    const char* pstr = lua_tolstring(L, 1, &len);
    strEvent = std::string(pstr, len);

    if (nArgs >= 2) {

        // format
        if (!lua_isnumber(L, 2)) {
            return luaL_error(L,
                              "vscp.getMeasurementValue: Argument error, "
                              "number expected: "
                              "vscp.getMeasurementValue( event[,format] ) ");
        }

        format = (int)lua_tointeger(L, 2);
    }

    if (0 == format) {
        if (!vscp_convertStringToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasurementValue: Failed to get "
                              "VSCP event from string!");
        }
    } else if (1 == format) {
        if (!vscp_convertXMLToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasurementValue: Failed to get "
                              "VSCP event from XML!");
        }
    } else if (2 == format) {
        if (!vscp_convertJSONToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasurementValue: Failed to get "
                              "VSCP event from JSON!");
        }
    }

    vscpEvent* pEvent = new vscpEvent;
    if (NULL == pEvent) {
        return luaL_error(L, "vscp.getMeasurementValue: Allocation error!");
    }

    pEvent->pdata = NULL;

    vscp_convertEventExToEvent(pEvent, &ex);
    vscp_getMeasurementAsDouble(&value, pEvent);
    vscp_deleteEvent(pEvent);

    lua_pushnumber(L, value);

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_get_MeasurementUnit
//

int
lua_get_MeasurementUnit(struct lua_State* L)
{
    vscpEventEx ex;
    int format = 0;
    std::string strEvent;

    int nArgs = lua_gettop(L);

    // Event
    if (!lua_isstring(L, 1)) {
        return luaL_error(L,
                          "vscp.getMeasurementUnit: Argument error, "
                          "string expected: "
                          "vscp.getMeasurementUnit( event[,format] ) ");
    }

    size_t len;
    const char* pstr = lua_tolstring(L, 1, &len);
    strEvent = std::string(pstr, len);

    if (nArgs >= 2) {

        // format
        if (!lua_isnumber(L, 2)) {
            return luaL_error(L,
                              "vscp.getMeasurementUnit: Argument error, "
                              "number expected: "
                              "vscp.getMeasurementUnit( event[,format] ) ");
        }

        format = (int)lua_tointeger(L, 2);
    }

    if (0 == format) {
        if (!vscp_convertStringToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasurementUnit: Failed to get "
                              "VSCP event from string!");
        }
    } else if (1 == format) {
        if (!vscp_convertXMLToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasurementUnit: Failed to get "
                              "VSCP event from XML!");
        }
    } else if (2 == format) {
        if (!vscp_convertJSONToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasurementUnit: Failed to get "
                              "VSCP event from JSON!");
        }
    }

    vscpEvent* pEvent = new vscpEvent;
    if (NULL == pEvent) {
        return luaL_error(L, "vscp.getMeasurementUnit: Allocation error!");
    }

    pEvent->pdata = NULL;

    vscp_convertEventExToEvent(pEvent, &ex);
    int unit = vscp_getMeasurementUnit(pEvent);
    vscp_deleteEvent(pEvent);

    lua_pushinteger(L, unit);

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_get_MeasurementSensorIndex
//

int
lua_get_MeasurementSensorIndex(struct lua_State* L)
{
    vscpEventEx ex;
    int format = 0;
    std::string strEvent;

    int nArgs = lua_gettop(L);

    // Event
    if (!lua_isstring(L, 1)) {
        return luaL_error(L,
                          "vscp.getMeasuremenSensorIndex: Argument error, "
                          "string expected: "
                          "vscp.getMeasuremenSensorIndex( event[,format] ) ");
    }

    size_t len;
    const char* pstr = lua_tolstring(L, 1, &len);
    strEvent = std::string(pstr, len);

    if (nArgs >= 2) {

        // format
        if (!lua_isnumber(L, 2)) {
            return luaL_error(
              L,
              "vscp.getMeasuremenSensorIndex: Argument "
              "error, number expected: "
              "vscp.getMeasuremenSensorIndex( event[,format] ) ");
        }

        format = (int)lua_tointeger(L, 2);
    }

    if (0 == format) {
        if (!vscp_convertStringToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasuremenSensorIndex: Failed to "
                              "get VSCP event from string!");
        }
    } else if (1 == format) {
        if (!vscp_convertXMLToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasuremenSensorIndex: Failed to get "
                              "VSCP event from XML!");
        }
    } else if (2 == format) {
        if (!vscp_convertJSONToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasuremenSensorIndex: Failed to get "
                              "VSCP event from JSON!");
        }
    }

    vscpEvent* pEvent = new vscpEvent;
    if (NULL == pEvent) {
        return luaL_error(L, "vscp.getMeasurementUnit: Allocation error!");
    }

    pEvent->pdata = NULL;

    vscp_convertEventExToEvent(pEvent, &ex);
    int sensorindex = vscp_getMeasurementSensorIndex(pEvent);
    vscp_deleteEvent(pEvent);

    lua_pushinteger(L, sensorindex);

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_get_MeasurementZone
//

int
lua_get_MeasurementZone(struct lua_State* L)
{
    vscpEventEx ex;
    int format = 0;
    std::string strEvent;

    int nArgs = lua_gettop(L);

    // Event
    if (!lua_isstring(L, 1)) {
        return luaL_error(L,
                          "vscp.getMeasurementZone: Argument error, "
                          "string expected: "
                          "vscp.getMeasurementZone( event[,format] ) ");
    }

    size_t len;
    const char* pstr = lua_tolstring(L, 1, &len);
    strEvent = std::string(pstr, len);

    if (nArgs >= 2) {

        // format
        if (!lua_isnumber(L, 2)) {
            return luaL_error(L,
                              "vscp.getMeasurementZone: Argument error, "
                              "number expected: "
                              "vscp.getMeasurementZone( event[,format] ) ");
        }

        format = (int)lua_tointeger(L, 2);
    }

    if (0 == format) {
        if (!vscp_convertStringToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasurementZone: Failed to get "
                              "VSCP event from string!");
        }
    } else if (1 == format) {
        if (!vscp_convertXMLToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasurementZone: Failed to get "
                              "VSCP event from XML!");
        }
    } else if (2 == format) {
        if (!vscp_convertJSONToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasurementZone: Failed to get "
                              "VSCP event from JSON!");
        }
    }

    vscpEvent* pEvent = new vscpEvent;
    if (NULL == pEvent) {
        return luaL_error(L, "vscp.getMeasurementZone: Allocation error!");
    }

    pEvent->pdata = NULL;

    vscp_convertEventExToEvent(pEvent, &ex);
    int zone = vscp_getMeasurementZone(pEvent);
    vscp_deleteEvent(pEvent);

    lua_pushinteger(L, zone);

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_get_MeasurementSubZone
//

int
lua_get_MeasurementSubZone(struct lua_State* L)
{
    vscpEventEx ex;
    int format = 0;
    std::string strEvent;

    int nArgs = lua_gettop(L);

    // Event
    if (!lua_isstring(L, 1)) {
        return luaL_error(L,
                          "vscp.getMeasurementSubZone: Argument error, "
                          "string expected: "
                          "vscp.getMeasurementSubZone( event[,format] ) ");
    }

    size_t len;
    const char* pstr = lua_tolstring(L, 1, &len);
    strEvent = std::string(pstr, len);

    if (nArgs >= 2) {

        // format
        if (!lua_isnumber(L, 2)) {
            return luaL_error(L,
                              "vscp.getMeasurementSubZone: Argument error,"
                              " number expected: "
                              "vscp.getMeasurementSubZone( event[,format] ) ");
        }

        format = (int)lua_tointeger(L, 2);
    }

    if (0 == format) {
        if (!vscp_convertStringToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasurementSubZone: Failed to get "
                              "VSCP event from string!");
        }
    } else if (1 == format) {
        if (!vscp_convertXMLToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasurementSubZone: Failed to get "
                              "VSCP event from XML!");
        }
    } else if (2 == format) {
        if (!vscp_convertJSONToEventEx(&ex, strEvent)) {
            return luaL_error(L,
                              "vscp.getMeasurementSubZone: Failed to get "
                              "VSCP event from JSON!");
        }
    }

    vscpEvent* pEvent = new vscpEvent;
    if (NULL == pEvent) {
        return luaL_error(L, "vscp.getMeasurementSubZone: Allocation error!");
    }

    pEvent->pdata = NULL;

    vscp_convertEventExToEvent(pEvent, &ex);
    int subzone = vscp_getMeasurementSubZone(pEvent);
    vscp_deleteEvent(pEvent);

    lua_pushinteger(L, subzone);

    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_connect
//
// Connect to remote server
//

int
lua_tcpip_connect(struct lua_State* L)
{
    // web_connect_client
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_connect_ssl
//
// Connect to remote server using SSL
//

int
lua_tcpip_connect_ssl(struct lua_State* L)
{
    // web_connect_client_secure
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_connect_info
//
// Get connection info
//      remote address
//      remote port
//      server address
//      server port
//      bSSL
//

int
lua_tcpip_connect_info(struct lua_State* L)
{
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_close
//
// Close connection to remote server
//

int
lua_tcpip_close(struct lua_State* L)
{
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_write
//
// Write data remote sever
//

int
lua_tcpip_write(struct lua_State* L)
{
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_read
//
// Read data from remote sever
//

int
lua_tcpip_read(struct lua_State* L)
{
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_get_response
//
// Wait for response from remote sever
//

int
lua_tcpip_get_response(struct lua_State* L)
{
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_download
//
// Download data from remote web server
//

int
lua_tcpip_download(struct lua_State* L)
{
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_get_httpd_version
//
// Get version for httpd code
//

int
lua_get_httpd_version(struct lua_State* L)
{
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_url_decode
//
// URL-decode input buffer into destination buffer.
//

int
lua_url_decode(struct lua_State* L)
{
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_url_encode
//
// URL-encode input buffer into destination buffer.
//

int
lua_url_encode(struct lua_State* L)
{
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_websocket_connect
//
// Connect to remote websocket client.
//

int
lua_websocket_connect(struct lua_State* L)
{
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_websocket_write
//
// URL-encode input buffer into destination buffer.
//

int
lua_websocket_write(struct lua_State* L)
{
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_websocket_read
//
// Read data from remote websocket host
//

int
lua_websocket_read(struct lua_State* L)
{
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// lua_websocket_lock
//
// lock connection
//

int
lua_websocket_lock(struct lua_State* L)
{
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// js_websocket_unlock
//
// unlock connection
//

int
js_websocket_unlock(struct lua_State* L)
{
    return 1;
}

///////////////////////////////////////////////////////////////////////////////
// js_md5
//
// Calculate md5 digest
//

int
js_md5(struct lua_State* L)
{
    return 1;
}