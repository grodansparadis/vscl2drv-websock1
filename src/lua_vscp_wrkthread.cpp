// vscp_Lua.cpp
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

#include <list>
#include <string>

#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

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

#include "civetweb_private_lua.h"
#include "civetweb.h"
#include "civetweb_lua.h"

#include <actioncodes.h>
#include <controlobject.h>
#include <lua_vscp_func.h>
#include <lua_vscp_wrkthread.h>
#include <userlist.h>
#include <version.h>
#include <vscp.h>
#include <vscp_debug.h>
#include <vscpdatetime.h>
#include <vscpdb.h>
#include <vscphelper.h>
#include <vscpremotetcpif.h>


///////////////////////////////////////////////////
//                 GLOBALS
///////////////////////////////////////////////////

extern CControlObject *gpobj;

///////////////////////////////////////////////////
//                   KEYS
///////////////////////////////////////////////////
const char lua_vscp__regkey_clientitem = 100;


///////////////////////////////////////////////////////////////////////////////
// actionLuaObj
//
// This thread executes a Lua
//

actionLuaObj::actionLuaObj(std::string &strScript)
{
    // OutputDebugString( "actionThreadURL: Create");
    m_strScript = strScript; // Script to execute
}

actionLuaObj::~actionLuaObj() {}

///////////////////////////////////////////////////////////////////////////////
// actionLuaThread
//
//
// TODO
void *
actionLuaThread(void *pData)
{
    // struct lua_State *L;
    // int lua_ret;
    // const char *lua_err_txt;

    // actionLuaObj *pobj = (actionLuaObj *)pData;
    // if (NULL == pobj) {
    //     syslog(LOG_ERR,
    //            "[LUA execution] - "
    //            "No control object, can't execute code.");
    //     return NULL;
    // }

    // pobj->m_start = vscpdatetime::Now(); // Mark start time

    // // Create new Lua context
    // L = luaL_newstate();

    // // Check if OK
    // if (!L) {
    //     // Failure
    //     return NULL;
    // }

    // // lua_pushlstring( L, const char *s, size_t len);

    // luaL_openlibs(L);

    // // Standard libs
    // //      luaopen_base,
    // //      luaopen_package,
    // //      luaopen_coroutine,
    // //      luaopen_table,
    // //      luaopen_io,
    // //      luaopen_os,
    // //      luaopen_string,
    // //      luaopen_math,
    // //      luaopen_utf8,
    // //      luaopen_debug,

    // // lsqlite3 - Sqlite3
    // // LuaXML_lib - XML support
    // // lfs - File system support
    // civetweb_open_lua_libs(L);

    // lua_newtable(L);

    // reg_string(L, "lua_type", "dmscript");
    // reg_string(L, "version", VSCPD_DISPLAY_VERSION);

    // std::string strFeedEvent;
    // if (vscp_convertEventToEventEx(&pobj->m_feedEvent, strFeedEvent)) {
    //     reg_string(L, "feedevent_str", (const char *)strFeedEvent.c_str());
    // } else {
    //     syslog(LOG_ERR, "Failed to convert Lua feed event to string.");
    // }

    // if (vscp_convertEventExToJSON(strFeedEvent,&pobj->m_feedEvent)) {
    //     reg_string(L, "feedevent_json", (const char *)strFeedEvent.c_str());
    // } else {
    //     syslog(LOG_ERR, "Failed to convert Lua feed event to JSON.");
    // }

    // if (vscp_convertEventExToXML(&pobj->m_feedEvent,strFeedEvent)) {
    //     mg_reg_string(L, "feedevent_xml", (const char *)strFeedEvent.c_str());
    // } else {
    //     syslog(LOG_ERR, "Failed to conver Lua feed event to XML.");
    // }

    // // From httpd
    // reg_function(L, "md5", web_lsp_md5);
    // reg_function(L, "get_time", web_lsp_get_time);
    // reg_function(L, "random", web_lsp_random);
    // reg_function(L, "uuid", web_lsp_uuid);

    // reg_function(L, "print", lua_vscp_print);
    // reg_function(L, "log", lua_vscp_log);
    // reg_function(L, "sleep", lua_vscp_sleep);

    // reg_function(L, "base64encode", lua_vscp_base64_encode);
    // reg_function(L, "base64decode", lua_vscp_base64_decode);

    // reg_function(L, "readvariable", lua_vscp_readVariable);
    // reg_function(L, "writevariable", lua_vscp_writeVariable);
    // reg_function(L, "writevariablevalue", lua_vscp_writeVariableValue);
    // reg_function(L, "deletevariable", lua_vscp_deleteVariable);

    // reg_function(
    //   L, "isVariableBase64Encoded", lua_vscp_isVariableBase64Encoded);
    // reg_function(L, "isVariablePersistent", lua_vscp_isVariablePersistent);
    // reg_function(L, "isVariableNumerical", lua_vscp_isVariableNumerical);
    // reg_function(L, "isStockVariable", lua_vscp_isStockVariable);

    // reg_function(L, "sendevent", lua_vscp_sendEvent);
    // reg_function(L, "receiveevent", lua_vscp_getEvent);
    // reg_function(L, "countevent", lua_vscp_getCountEvent);
    // reg_function(L, "setfilter", lua_vscp_setFilter);

    // reg_function(L, "ismeasurement", lua_is_Measurement);
    // reg_function(L, "sendmeasurement", lua_send_Measurement);
    // reg_function(L, "getmeasurementvalue", lua_get_MeasurementValue);
    // reg_function(L, "getmeasurementunit", lua_get_MeasurementUnit);
    // reg_function(
    //   L, "getmeasurementsensorindex", lua_get_MeasurementSensorIndex);
    // reg_function(L, "getmeasurementzone", lua_get_MeasurementZone);
    // reg_function(L, "getmeasurementsubzone", lua_get_MeasurementSubZone);

    // /*
    //     // Save the DM feed event for easy access
    //     std::string strEvent;
    //     vscp_convertEventExToJSON( &m_feedEvent, strEvent );
    //     duk_push_string( ctx, (const char *)strEvent.c_str() );
    //     duk_json_decode(ctx, -1);
    //     duk_put_global_string(ctx, "vscp_feedevent");

    //     // Save client object as a global pointer
    //     duk_push_pointer(ctx, (void *)pobj->m_pClientItem );
    //     duk_put_global_string(ctx, "vscp_controlobject");

    //     // Create VSCP client
    //     pobj->m_pClientItem = new CClientItem();
    //     vscp_clearVSCPFilter( &pobj->m_pClientItem->m_filter );

    //     // Save the client object as a global pointer
    //     duk_push_pointer(ctx, (void *)pobj->m_pClientItem );
    //     duk_put_global_string(ctx, "vscp_clientitem");

    //     // reading [global object].vscp_clientItem
    //     duk_push_global_object(ctx);                // -> stack: [ global ]
    //     duk_push_string(ctx, "vscp_clientitem");    // -> stack: [ global
    //    "vscp_clientItem" ] duk_get_prop(ctx, -2);                      // ->
    //    stack: [ global vscp_clientItem ] CClientItem *pItem = (CClientItem
    //    *)duk_get_pointer(ctx,-1); std::string user = pItem->m_UserName;

    //     duk_bool_t rc;

    //   */

    // if (pf_uuid_generate.f) {
    //     web_reg_function(L, "uuid", web_lsp_uuid);
    // }

    // // Create VSCP client
    // pobj->m_pClientItem = new CClientItem();
    // if (NULL != pobj->m_pClientItem) return NULL;
    // vscp_clearVSCPFilter(&pobj->m_pClientItem->m_filter);

    // // This is an active client
    // pobj->m_pClientItem->m_bOpen = false;
    // pobj->m_pClientItem->m_type  = CLIENT_ITEM_INTERFACE_TYPE_CLIENT_LUA;
    // pobj->m_pClientItem->m_strDeviceName = ("Internal daemon Lua client.");
    // pobj->m_pClientItem->m_strDeviceName += ("|Started at ");
    // pobj->m_pClientItem->m_strDeviceName +=
    //   vscpdatetime::Now().getISODateTime();

    // // Add the client to the Client List
    // pthread_mutex_lock(&gpobj->m_clientList.m_mutexItemList);
    // if (!gpobj->addClient(pobj->m_pClientItem)) {
    //     // Failed to add client
    //     delete pobj->m_pClientItem;
    //     pobj->m_pClientItem = NULL;
    //     pthread_mutex_unlock(&gpobj->m_clientList.m_mutexItemList);
    //     syslog(LOG_ERR,
    //            "LUA worker: Failed to add client. Terminating thread.");
    //     return NULL;
    // }
    // pthread_mutex_unlock(&gpobj->m_clientList.m_mutexItemList);

    // // Open the channel
    // pobj->m_pClientItem->m_bOpen = true;

    // // Register client item object
    // // lua_pushlightuserdata( L, (void *)&lua_vscp__regkey_clientitem );
    // lua_pushlstring(L, "vscp_clientitem", 15);
    // lua_pushlightuserdata(L, (void *)pobj->m_pClientItem);
    // lua_settable(L, LUA_REGISTRYINDEX);

    // lua_setglobal(L, "vscp");

    // // Execute the Lua
    // // web_run_lua_string_script( (const char *)m_xxstrScript.c_str() );
    // // luaL_dostring( lua, (const char *)m_xxstrScript.c_str() );
    // /*if ( 0 != duk_peval( ctx ) ) {
    //     std::string strError =
    //             vscp_str_format( "Lua failed to execute: %s\n",
    //                                 duk_safe_to_string(ctx, -1) );
    //     syslog( LOG_ERR, ( strError, DAEMON_LOGMSG_NORMAL, DAEMON_LOGTYPE_DM );
    // }*/

    // web_open_lua_libs(L);

    // lua_ret = luaL_loadstring(L, (const char *)pobj->m_strScript.c_str());

    // if (lua_ret != LUA_OK) {

    //     syslog(LOG_ERR,
    //            "Lua failed to load script from"
    //            " DM parameter. Script = %s",
    //            (const char *)pobj->m_strScript.c_str());

    //     return NULL;
    // }

    // // The script file is loaded, now call it
    // lua_ret = lua_pcall(L,
    //                     0, // no arguments
    //                     1, // zero or one return value
    //                     0  // errors as string return value
    // );

    // if (lua_ret != LUA_OK) {

    //     // Error when executing the script
    //     lua_err_txt = lua_tostring(L, -1);

    //     syslog(LOG_ERR,
    //            "Error running Lua script. "
    //            "Error = %s : Script = %s\n",
    //            lua_err_txt,
    //            (const char *)pobj->m_strScript.c_str());

    //     return NULL;
    // }

    // //	lua_close(L); must be done somewhere else

    // // If the script wants to log results it can do so
    // // by itself with the log function

    // // duk_pop(ctx);  // pop eval. result

    // // Close the channel
    // pobj->m_pClientItem->m_bOpen = false;

    // // Remove client and session item
    // pthread_mutex_lock(&gpobj->m_clientList.m_mutexItemList);
    // gpobj->removeClient(pobj->m_pClientItem);
    // pobj->m_pClientItem = NULL;
    // pthread_mutex_unlock(&gpobj->m_clientList.m_mutexItemList);

    // // Destroy the Lua context
    // // duk_destroy_heap( ctx );

    // pobj->m_stop = vscpdatetime::Now(); // Mark stop time

    return NULL;
}
