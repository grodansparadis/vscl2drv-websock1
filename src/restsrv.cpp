// restsrv.cpp
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

#ifdef __GNUG__
//#pragma implementation
#endif

#define _POSIX

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "web_css.h"
#include "web_js.h"
#include "web_template.h"

#include <json.hpp> // Needs C++11  -std=c++11

#include <actioncodes.h>
#include <civetweb.h>
#include <controlobject.h>
#include <devicelist.h>
#include <devicethread.h>
#include <mdf.h>
#include <version.h>
#include <vscp.h>
#include <vscp_aes.h>
#include <vscp_debug.h>
#include <vscphelper.h>
#include <websrv.h>

#include "restsrv.h"

// https://github.com/nlohmann/json
using json = nlohmann::json;

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

///////////////////////////////////////////////////
//                 GLOBALS
///////////////////////////////////////////////////

extern CControlObject* gpobj;

// Prototypes

void
restsrv_doStatus(struct mg_connection* conn,
                 struct restsrv_session* pSession,
                 int format);

void
restsrv_doOpen(struct mg_connection* conn,
               struct restsrv_session* pSession,
               int format);

void
restsrv_doClose(struct mg_connection* conn,
                struct restsrv_session* pSession,
                int format);

void
restsrv_doSendEvent(struct mg_connection* conn,
                    struct restsrv_session* pSession,
                    int format,
                    vscpEvent* pEvent);

void
restsrv_doReceiveEvent(struct mg_connection* conn,
                       struct restsrv_session* pSession,
                       int format,
                       size_t count);

void
restsrv_doReceiveEvent(struct mg_connection* conn,
                       struct restsrv_session* pSession,
                       int format,
                       size_t count);

void
restsrv_doSetFilter(struct mg_connection* conn,
                    struct restsrv_session* pSession,
                    int format,
                    vscpEventFilter& vscpfilter);

void
restsrv_doClearQueue(struct mg_connection* conn,
                     struct restsrv_session* pSession,
                     int format);

void
restsrv_doListVariable(struct mg_connection* conn,
                       struct restsrv_session* pSession,
                       int format,
                       std::string& strRegEx,
                       bool bShort);

void
restsrv_doListVariable(struct mg_connection* conn,
                       struct restsrv_session* pSession,
                       int format,
                       std::string& strRegEx,
                       bool bShort);

void
restsrv_doReadVariable(struct mg_connection* conn,
                       struct restsrv_session* pSession,
                       int format,
                       std::string& strVariableName);

void
restsrv_doWriteVariable(struct mg_connection* conn,
                        struct restsrv_session* pSession,
                        int format,
                        std::string& strVariableName,
                        std::string& strValue);

void
restsrv_doCreateVariable(struct mg_connection* conn,
                         struct restsrv_session* pSession,
                         int format,
                         std::string& strVariable,
                         std::string& strType,
                         std::string& strValue,
                         std::string& strPersistent,
                         std::string& strAccessRight,
                         std::string& strNote);

void
restsrv_doDeleteVariable(struct mg_connection* conn,
                         struct restsrv_session* pSession,
                         int format,
                         std::string& strVariable);

void
restsrv_doWriteMeasurement(struct mg_connection* conn,
                           struct restsrv_session* pSession,
                           int format,
                           std::string& strDateTime,
                           std::string& strGuid,
                           std::string& strLevel,
                           std::string& strType,
                           std::string& strValue,
                           std::string& strUnit,
                           std::string& strSensorIdx,
                           std::string& strZone,
                           std::string& strSubZone,
                           std::string& strEventFormat);

void
websrc_renderTableData(struct mg_connection* conn,
                       struct restsrv_session* pSession,
                       int format,
                       std::string& strName,
                       struct _vscpFileRecord* pRecords,
                       long nfetchedRecords);

void
restsrv_doFetchMDF(struct mg_connection* conn,
                   struct restsrv_session* pSession,
                   int format,
                   std::string& strURL);

void
websrc_renderTableData(struct mg_connection* conn,
                       struct restsrv_session* pSession,
                       int format,
                       std::string& strName,
                       struct _vscpFileRecord* pRecords,
                       long nfetchedRecords);

void
restsrv_doGetTableData(struct mg_connection* conn,
                       struct restsrv_session* pSession,
                       int format,
                       std::string& strName,
                       std::string& strFrom,
                       std::string& strTo);

const char* rest_errors[][REST_FORMAT_COUNT + 1] = {
    { REST_PLAIN_ERROR_SUCCESS,
      REST_CSV_ERROR_SUCCESS,
      REST_XML_ERROR_SUCCESS,
      REST_JSON_ERROR_SUCCESS,
      REST_JSONP_ERROR_SUCCESS },
    { REST_PLAIN_ERROR_GENERAL_FAILURE,
      REST_CSV_ERROR_GENERAL_FAILURE,
      REST_XML_ERROR_GENERAL_FAILURE,
      REST_JSON_ERROR_GENERAL_FAILURE,
      REST_JSONP_ERROR_GENERAL_FAILURE },
    { REST_PLAIN_ERROR_INVALID_SESSION,
      REST_CSV_ERROR_INVALID_SESSION,
      REST_XML_ERROR_INVALID_SESSION,
      REST_JSON_ERROR_INVALID_SESSION,
      REST_JSONP_ERROR_INVALID_SESSION },
    { REST_PLAIN_ERROR_UNSUPPORTED_FORMAT,
      REST_CSV_ERROR_UNSUPPORTED_FORMAT,
      REST_XML_ERROR_UNSUPPORTED_FORMAT,
      REST_JSON_ERROR_UNSUPPORTED_FORMAT,
      REST_JSONP_ERROR_UNSUPPORTED_FORMAT },
    { REST_PLAIN_ERROR_COULD_NOT_OPEN_SESSION,
      REST_CSV_ERROR_COULD_NOT_OPEN_SESSION,
      REST_XML_ERROR_COULD_NOT_OPEN_SESSION,
      REST_JSON_ERROR_COULD_NOT_OPEN_SESSION,
      REST_JSONP_ERROR_COULD_NOT_OPEN_SESSION },
    { REST_PLAIN_ERROR_MISSING_DATA,
      REST_CSV_ERROR_MISSING_DATA,
      REST_XML_ERROR_MISSING_DATA,
      REST_JSON_ERROR_MISSING_DATA,
      REST_JSONP_ERROR_MISSING_DATA },
    { REST_PLAIN_ERROR_INPUT_QUEUE_EMPTY,
      REST_CSV_ERROR_INPUT_QUEUE_EMPTY,
      REST_XML_ERROR_INPUT_QUEUE_EMPTY,
      REST_JSON_ERROR_INPUT_QUEUE_EMPTY,
      REST_JSONP_ERROR_INPUT_QUEUE_EMPTY },
    { REST_PLAIN_ERROR_VARIABLE_NOT_FOUND,
      REST_CSV_ERROR_VARIABLE_NOT_FOUND,
      REST_XML_ERROR_VARIABLE_NOT_FOUND,
      REST_JSON_ERROR_VARIABLE_NOT_FOUND,
      REST_JSONP_ERROR_VARIABLE_NOT_FOUND },
    { REST_PLAIN_ERROR_VARIABLE_NOT_CREATED,
      REST_CSV_ERROR_VARIABLE_NOT_CREATED,
      REST_XML_ERROR_VARIABLE_NOT_CREATED,
      REST_JSON_ERROR_VARIABLE_NOT_CREATED,
      REST_JSONP_ERROR_VARIABLE_NOT_CREATED },
    { REST_PLAIN_ERROR_VARIABLE_FAIL_UPDATE,
      REST_CSV_ERROR_VARIABLE_FAIL_UPDATE,
      REST_XML_ERROR_VARIABLE_FAIL_UPDATE,
      REST_JSON_ERROR_VARIABLE_FAIL_UPDATE,
      REST_JSONP_ERROR_VARIABLE_FAIL_UPDATE },
    { REST_PLAIN_ERROR_NO_ROOM,
      REST_CSV_ERROR_NO_ROOM,
      REST_XML_ERROR_NO_ROOM,
      REST_JSON_ERROR_NO_ROOM,
      REST_JSONP_ERROR_NO_ROOM },
    {
      REST_PLAIN_ERROR_TABLE_NOT_FOUND,
      REST_CSV_ERROR_TABLE_NOT_FOUND,
      REST_XML_ERROR_TABLE_NOT_FOUND,
      REST_JSON_ERROR_TABLE_NOT_FOUND,
      REST_JSONP_ERROR_TABLE_NOT_FOUND,
    },
    { REST_PLAIN_ERROR_TABLE_NO_DATA,
      REST_CSV_ERROR_TABLE_NO_DATA,
      REST_XML_ERROR_TABLE_NO_DATA,
      REST_JSON_ERROR_TABLE_NO_DATA,
      REST_JSONP_ERROR_TABLE_NO_DATA },
    { REST_PLAIN_ERROR_TABLE_RANGE,
      REST_CSV_ERROR_TABLE_RANGE,
      REST_XML_ERROR_TABLE_RANGE,
      REST_JSON_ERROR_TABLE_RANGE,
      REST_JSONP_ERROR_TABLE_RANGE },
    { REST_PLAIN_ERROR_INVALID_USER,
      REST_CSV_ERROR_INVALID_USER,
      REST_XML_ERROR_INVALID_USER,
      REST_JSON_ERROR_INVALID_USER,
      REST_JSONP_ERROR_INVALID_USER },
    { REST_PLAIN_ERROR_INVALID_ORIGIN,
      REST_CSV_ERROR_INVALID_ORIGIN,
      REST_XML_ERROR_INVALID_ORIGIN,
      REST_JSON_ERROR_INVALID_ORIGIN,
      REST_JSONP_ERROR_INVALID_ORIGIN },
    { REST_PLAIN_ERROR_INVALID_PASSWORD,
      REST_CSV_ERROR_INVALID_PASSWORD,
      REST_XML_ERROR_INVALID_PASSWORD,
      REST_JSON_ERROR_INVALID_PASSWORD,
      REST_JSONP_ERROR_INVALID_PASSWORD },
    { REST_PLAIN_ERROR_MEMORY,
      REST_CSV_ERROR_MEMORY,
      REST_XML_ERROR_MEMORY,
      REST_JSON_ERROR_MEMORY,
      REST_JSONP_ERROR_MEMORY },
    { REST_PLAIN_ERROR_VARIABLE_NOT_DELETE,
      REST_CSV_ERROR_VARIABLE_NOT_DELETE,
      REST_XML_ERROR_VARIABLE_NOT_DELETE,
      REST_JSON_ERROR_VARIABLE_NOT_DELETE,
      REST_JSONP_ERROR_VARIABLE_NOT_DELETE },

};

//-----------------------------------------------------------------------------
//                                   REST
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
// restsrv_error
//

void
restsrv_error(struct mg_connection* conn,
              struct restsrv_session* pSession,
              int format,
              int errorcode)
{
    int returncode = 200;

    if (__VSCP_DEBUG_REST) {
        syslog(LOG_DEBUG,
               "REST: error format=%d errorcode=%d",
               format,
               errorcode);
    }

    if (REST_FORMAT_PLAIN == format) {

        websrv_sendheader(conn, returncode, REST_MIME_TYPE_PLAIN);
        mg_write(conn,
                 rest_errors[errorcode][REST_FORMAT_PLAIN],
                 strlen(rest_errors[errorcode][REST_FORMAT_PLAIN]));
        mg_write(conn, "", 0); // Terminator
        return;

    } else if (REST_FORMAT_CSV == format) {

        websrv_sendheader(conn, returncode, REST_MIME_TYPE_CSV);
        mg_write(conn,
                 rest_errors[errorcode][REST_FORMAT_CSV],
                 strlen(rest_errors[errorcode][REST_FORMAT_CSV]));
        mg_write(conn, "", 0); // Terminator
        return;

    } else if (REST_FORMAT_XML == format) {

        websrv_sendheader(conn, returncode, REST_MIME_TYPE_XML);
        mg_write(conn, XML_HEADER, strlen(XML_HEADER));
        mg_write(conn,
                 rest_errors[errorcode][REST_FORMAT_XML],
                 strlen(rest_errors[errorcode][REST_FORMAT_XML]));
        mg_write(conn, "", 0); // Terminator
        return;

    } else if (REST_FORMAT_JSON == format) {

        websrv_sendheader(conn, returncode, REST_MIME_TYPE_JSON);
        mg_write(conn,
                 rest_errors[errorcode][REST_FORMAT_JSON],
                 strlen(rest_errors[errorcode][REST_FORMAT_JSON]));
        mg_write(conn, "", 0); // Terminator
        return;

    } else if (REST_FORMAT_JSONP == format) {

        websrv_sendheader(conn, returncode, REST_MIME_TYPE_JSONP);
        mg_write(conn,
                 rest_errors[errorcode][REST_FORMAT_JSONP],
                 strlen(rest_errors[errorcode][REST_FORMAT_JSONP]));
        mg_write(conn, "", 0); // Terminator
        return;

    } else {

        websrv_sendheader(conn, returncode, REST_MIME_TYPE_PLAIN);
        mg_write(conn,
                 REST_PLAIN_ERROR_UNSUPPORTED_FORMAT,
                 strlen(REST_PLAIN_ERROR_UNSUPPORTED_FORMAT));
        mg_write(conn, "", 0); // Terminator
        return;
    }
}

///////////////////////////////////////////////////////////////////////////////
// restsrv_sendHeader
//

void
restsrv_sendHeader(struct mg_connection* conn, int format, int returncode)
{
    if (REST_FORMAT_PLAIN == format) {
        websrv_sendheader(conn, returncode, REST_MIME_TYPE_PLAIN);
    } else if (REST_FORMAT_CSV == format) {
        websrv_sendheader(conn, returncode, REST_MIME_TYPE_CSV);
    } else if (REST_FORMAT_XML == format) {
        websrv_sendheader(conn, returncode, REST_MIME_TYPE_XML);
    } else if (REST_FORMAT_JSON == format) {
        websrv_sendheader(conn, returncode, REST_MIME_TYPE_JSON);
    } else if (REST_FORMAT_JSONP == format) {
        websrv_sendheader(conn, returncode, REST_MIME_TYPE_JSONP);
    }
}

///////////////////////////////////////////////////////////////////////////////
// restsrv_get_session
//

struct restsrv_session*
restsrv_get_session(struct mg_connection* conn, std::string& sid)
{
    const struct mg_request_info* reqinfo;

    // Check pointers
    if (!conn || !(reqinfo = mg_get_request_info(conn))) {
        syslog(LOG_ERR, "REST: get_session, Pointer error.");
        return NULL;
    }

    if (0 == sid.length()) {
        syslog(LOG_ERR, "REST: get_session, sid length is zero.");
        return NULL;
    }

    // find existing session
    pthread_mutex_lock(&gpobj->m_mutex_restSession);
    std::list<struct restsrv_session*>::iterator iter;
    for (iter = gpobj->m_rest_sessions.begin();
         iter != gpobj->m_rest_sessions.end();
         ++iter) {
        struct restsrv_session* pSession = *iter;
        if (0 == strcmp((const char*)sid.c_str(), pSession->m_sid)) {
            pSession->m_lastActiveTime = time(NULL);
            pthread_mutex_unlock(&gpobj->m_mutex_restSession);
            if (__VSCP_DEBUG_REST) {
                syslog(LOG_DEBUG, "REST: get_session, Session found.");
            }
            return pSession;
        }
    }
    pthread_mutex_unlock(&gpobj->m_mutex_restSession);

    syslog(LOG_ERR, "REST: get_session, Session not found.");
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// restsrv_add_session
//

restsrv_session*
restsrv_add_session(struct mg_connection* conn, CUserItem* pUserItem)
{
    std::string user;
    struct restsrv_session* pSession;
    const struct mg_request_info* reqinfo;

    // Check pointers
    if (!conn || !(reqinfo = mg_get_request_info(conn))) {
        syslog(LOG_ERR, "REST: add_session, Pointer error.");
        return 0;
    }

    // !!!!! This blokc is not used at the moment !!!
    // Parse "Authorization:" header, fail fast on parse error
    if (0) {
        const char* pheader = mg_get_header(conn, "Authorization");
        if (NULL == pheader) {
            syslog(LOG_ERR, "REST: add_session, No 'Authorization' header.");
            return NULL;
        }

        std::map<std::string, std::string> hdrmap;
        std::string header = std::string(pheader);
        websrv_parseHeader(hdrmap, header);

        // Get username
        if (!websrv_getHeaderElement(hdrmap, "username", user)) {
            syslog(LOG_ERR, "REST: add_session, no 'username' in header.");
            return NULL;
        }
    }

    // Create fresh session
    pSession = new struct restsrv_session;
    if (NULL == pSession) {
        syslog(LOG_ERR, "REST: add_session, unable to create session object.");
        return NULL;
    }
    memset(pSession, 0, sizeof(websrv_session));

    // Generate a random session ID
    unsigned char iv[16];
    char hexiv[33];
    getRandomIV(iv, 16); // Generate 16 random bytes
    memset(hexiv, 0, sizeof(hexiv));
    vscp_byteArray2HexStr(hexiv, iv, 16);

    memset(pSession->m_sid, 0, sizeof(pSession->m_sid));
    memcpy(pSession->m_sid, hexiv, 32);

    pSession->m_lastActiveTime = time(NULL);

    pSession->m_pClientItem = new CClientItem(); // Create client
    if (NULL == pSession->m_pClientItem) {
        syslog(LOG_ERR,
               "[restsrv] New session: Unable to create client object.");
        delete pSession;
        return NULL;
    }

    // Set client data
    pSession->m_pClientItem->bAuthenticated = true; // Authenticated
    pSession->m_pClientItem->m_pUserItem    = pUserItem;
    vscp_clearVSCPFilter(&pSession->m_pClientItem->m_filter); // Clear filter
    pSession->m_pClientItem->m_bOpen = false; // Start out closed
    pSession->m_pClientItem->m_dtutc = vscpdatetime::Now();
    pSession->m_pClientItem->m_type =
      CLIENT_ITEM_INTERFACE_TYPE_CLIENT_WEBSOCKET;
    pSession->m_pClientItem->m_strDeviceName = ("Internal REST server client.");

    // Add the client to the Client List
    pthread_mutex_lock(&gpobj->m_clientList.m_mutexItemList);
    if (!gpobj->addClient(pSession->m_pClientItem)) {
        // Failed to add client
        delete pSession->m_pClientItem;
        pSession->m_pClientItem = NULL;
        pthread_mutex_unlock(&gpobj->m_clientList.m_mutexItemList);
        syslog(LOG_ERR,
               "REST server: new session, Failed to add client. Terminating "
               "thread.");
        return NULL;
    }
    pthread_mutex_unlock(&gpobj->m_clientList.m_mutexItemList);

    // Add to linked list
    pthread_mutex_lock(&gpobj->m_mutex_restSession);
    gpobj->m_rest_sessions.push_back(pSession);
    pthread_mutex_unlock(&gpobj->m_mutex_restSession);

    return pSession;
}

///////////////////////////////////////////////////////////////////////////////
// websrv_expire_sessions
//

void
restsrv_expire_sessions(struct mg_connection* conn)
{
    time_t now;

    now = time(NULL);

    pthread_mutex_lock(&gpobj->m_mutex_restSession);

    std::list<struct restsrv_session*>::iterator it;

    try {
        for (it = gpobj->m_rest_sessions.begin();
             it != gpobj->m_rest_sessions.end();
             /* inline */) {
            struct restsrv_session* pSession = *it;
            if ((now - pSession->m_lastActiveTime) > (60 * 60)) {
                it = gpobj->m_rest_sessions.erase(it);
                if (__VSCP_DEBUG_REST) {
                    syslog(LOG_DEBUG, "REST: Session expired");
                }
                delete pSession;
            } else {
                ++it;
            }
        }
    } catch (...) {
        syslog(LOG_ERR, "Exception expire_session");
    }

    pthread_mutex_unlock(&gpobj->m_mutex_restSession);
}

///////////////////////////////////////////////////////////////////////////////
// websrv_restapi
//

int
websrv_restapi(struct mg_connection* conn, void* cbdata)
{
    char bufBody[32000]; // Buffer for body (POST) data
    int len_post_data;
    const char* pParams = NULL; // Pointer to query data or POST data
    int lenParam        = 0;
    char buf[2048];
    char date[64];
    std::string str;
    time_t curtime = time(NULL);
    long format    = REST_FORMAT_PLAIN;
    std::map<std::string, std::string> keypairs;
    struct mg_context* ctx;
    const struct mg_request_info* reqinfo;
    struct restsrv_session* pSession = NULL;
    CUserItem* pUserItem             = NULL;

    memset(bufBody, 0, sizeof(bufBody));

    // Check pointer
    if (!conn || !(ctx = mg_get_context(conn)) ||
        !(reqinfo = mg_get_request_info(conn))) {
        syslog(LOG_ERR, "REST: restapi - invalid pointers");
        return WEB_ERROR;
    }

    // Get method
    char method[33];
    memset(method, 0, sizeof(method));
    strncpy(method,
            reqinfo->request_method,
            std::min((int)strlen(reqinfo->request_method), 32));

    // Make string with GMT time
    vscp_getTimeString(date, sizeof(date), &curtime);

    // Set defaults
    keypairs["FORMAT"] = "plain";
    keypairs["OP"]     = "open";

    if (NULL != strstr(method, "POST")) {

        const char* pHeader;

        // read POST data
        len_post_data = mg_read(conn, bufBody, sizeof(bufBody));

        // user
        if (NULL != (pHeader = mg_get_header(conn, "vscpuser"))) {
            memset(buf, 0, sizeof(buf));
            strncpy(buf, pHeader, std::min(sizeof(buf) - 1, strlen(pHeader)));
            keypairs["VSCPUSER"] = std::string(buf);
        }

        // password
        if (NULL != (pHeader = mg_get_header(conn, "vscpsecret"))) {
            memset(buf, 0, sizeof(buf));
            strncpy(buf, pHeader, std::min(sizeof(buf) - 1, strlen(pHeader)));
            keypairs["VSCPSECRET"] = std::string(buf);
        }

        // session
        if (NULL != (pHeader = mg_get_header(conn, "vscpsession"))) {
            memset(buf, 0, sizeof(buf));
            strncpy(buf, pHeader, std::min(sizeof(buf) - 1, strlen(pHeader)));
            keypairs["VSCPSESSION"] = std::string(buf);
        }

        pParams  = bufBody; // Parameters is in the body
        lenParam = len_post_data;

    } else {

        // get parameters for get
        if (NULL != reqinfo->query_string) {

            if (0 < mg_get_var(reqinfo->query_string,
                               strlen(reqinfo->query_string),
                               "vscpuser",
                               buf,
                               sizeof(buf))) {
                keypairs["VSCPUSER"] = std::string(buf);
            }

            if (0 < mg_get_var(reqinfo->query_string,
                               strlen(reqinfo->query_string),
                               "vscpsecret",
                               buf,
                               sizeof(buf))) {
                keypairs["VSCPSECRET"] = std::string(buf);
            }

            if (0 < mg_get_var(reqinfo->query_string,
                               strlen(reqinfo->query_string),
                               "vscpsession",
                               buf,
                               sizeof(buf))) {
                keypairs["VSCPSESSION"] = std::string(buf);
            }

            pParams  = reqinfo->query_string; // Parameters is in query string
            lenParam = strlen(reqinfo->query_string);
        }
    }

    // format
    if (0 < mg_get_var(pParams, lenParam, "format", buf, sizeof(buf))) {
        keypairs["FORMAT"] = vscp_upper(std::string(buf));
    }

    // op
    if (0 < mg_get_var(pParams, lenParam, "op", buf, sizeof(buf))) {
        keypairs["OP"] = vscp_makeUpper_copy(std::string(buf));
    }

    // vscpevent
    if (0 < mg_get_var(pParams, lenParam, "vscpevent", buf, sizeof(buf))) {
        keypairs["VSCPEVENT"] = std::string(buf);
    }

    // count
    if (0 < mg_get_var(pParams, lenParam, "count", buf, sizeof(buf))) {
        keypairs["COUNT"] = std::string(buf);
    }

    // vscpfilter
    if (0 < mg_get_var(pParams, lenParam, "vscpfilter", buf, sizeof(buf))) {
        keypairs["VSCPFILTER"] = std::string(buf);
    }

    // vscpmask
    if (0 < mg_get_var(pParams, lenParam, "vscpmask", buf, sizeof(buf))) {
        keypairs["VSCPMASK"] = std::string(buf);
    }

    // variable
    if (0 < mg_get_var(pParams, lenParam, "variable", buf, sizeof(buf))) {
        keypairs["VARIABLE"] = std::string(buf);
    }

    // value
    if (0 < mg_get_var(pParams, lenParam, "value", buf, sizeof(buf))) {
        keypairs["VALUE"] = std::string(buf);
    }

    // type (number or string)
    if (0 < mg_get_var(pParams, lenParam, "type", buf, sizeof(buf))) {
        keypairs["TYPE"] = std::string(buf);
    }

    // persistent
    if (0 < mg_get_var(pParams, lenParam, "persistent", buf, sizeof(buf))) {
        keypairs["PERSISTENT"] = std::string(buf);
    }

    // access-right  (hex or decimal)
    if (0 < mg_get_var(pParams, lenParam, "accessright", buf, sizeof(buf))) {
        keypairs["ACCESSRIGHT"] = std::string(buf);
    }

    // note
    if (0 < mg_get_var(pParams, lenParam, "note", buf, sizeof(buf))) {
        keypairs["NOTE"] = std::string(buf);
    }

    // listlong
    if (0 < mg_get_var(pParams, lenParam, "listlong", buf, sizeof(buf))) {
        keypairs["LISTLONG"] = std::string(buf);
    }

    // regex
    if (0 < mg_get_var(pParams, lenParam, "regex", buf, sizeof(buf))) {
        keypairs[("REGEX")] = std::string(buf);
    }

    // unit
    if (0 < mg_get_var(pParams, lenParam, "unit", buf, sizeof(buf))) {
        keypairs["UNIT"] = std::string(buf);
    }

    // sensoridx
    if (0 < mg_get_var(pParams, lenParam, "sensoridx", buf, sizeof(buf))) {
        keypairs["SENSORINDEX"] = std::string(buf);
    }

    // level  ( VSCP level 1 or 2 )
    if (0 < mg_get_var(pParams, lenParam, "level", buf, sizeof(buf))) {
        keypairs["LEVEL"] = std::string(buf);
    }

    // zone
    if (0 < mg_get_var(pParams, lenParam, "zone", buf, sizeof(buf))) {
        keypairs["ZONE"] = std::string(buf);
    }

    // subzone
    if (0 < mg_get_var(pParams, lenParam, "subzone", buf, sizeof(buf))) {
        keypairs["SUBZONE"] = std::string(buf);
    }

    // guid
    if (0 < mg_get_var(pParams, lenParam, "guid", buf, sizeof(buf))) {
        keypairs["GUID"] = std::string(buf);
    }

    // name
    if (0 < mg_get_var(pParams, lenParam, "name", buf, sizeof(buf))) {
        keypairs["NAME"] = std::string(buf);
    }

    // from
    if (0 < mg_get_var(pParams, lenParam, "from", buf, sizeof(buf))) {
        keypairs["FROM"] = std::string(buf);
    }

    // to
    if (0 < mg_get_var(pParams, lenParam, "to", buf, sizeof(buf))) {
        keypairs["TO"] = std::string(buf);
    }

    // url
    if (0 < mg_get_var(pParams, lenParam, "url", buf, sizeof(buf))) {
        keypairs["URL"] = std::string(buf);
    }

    // eventformat
    if (0 < mg_get_var(pParams, lenParam, "eventformat", buf, sizeof(buf))) {
        keypairs["EVENTFORMAT"] = std::string(buf);
    }

    // datetime
    if (0 < mg_get_var(pParams, lenParam, "datetime", buf, sizeof(buf))) {
        keypairs["DATETIME"] = std::string(buf);
    }

    // Get format
    if ("PLAIN" == keypairs["FORMAT"]) {
        format = REST_FORMAT_PLAIN;
    } else if ("CSV" == keypairs["FORMAT"]) {
        format = REST_FORMAT_CSV;
    } else if ("XML" == keypairs["FORMAT"]) {
        format = REST_FORMAT_XML;
    } else if ("JSON" == keypairs["FORMAT"]) {
        format = REST_FORMAT_JSON;
    } else if ("JSONP" == keypairs["FORMAT"]) {
        format = REST_FORMAT_JSONP;
    } else {

        websrv_sendheader(conn, 400, "text/plain");
        mg_write(conn,
                 REST_PLAIN_ERROR_UNSUPPORTED_FORMAT,
                 strlen(REST_PLAIN_ERROR_UNSUPPORTED_FORMAT));
        mg_write(conn, "", 0); // Terminator

        if (__VSCP_DEBUG_REST) {
            syslog(LOG_DEBUG, "REST: restapi - invalid format ");
        }

        return WEB_ERROR;
    }

    // If we have a session key we try to get the session
    if (("") != keypairs["VSCPSESSION"]) {

        // Get session
        pSession = restsrv_get_session(conn, keypairs["VSCPSESSION"]);
    }

    if (NULL == pSession) {

        // Get user
        pUserItem = gpobj->m_userList.getUser(keypairs["VSCPUSER"]);

        // Check if user is valid
        if (NULL == pUserItem) {

            std::string strErr =
              vscp_str_format("[REST Client] Host [%s] Invalid user [%s]",
                              std::string(reqinfo->remote_addr).c_str(),
                              (const char*)keypairs[("VSCPUSER")].c_str());

            syslog(LOG_ERR, "%s", strErr.c_str());

            restsrv_error(conn, pSession, format, REST_ERROR_CODE_INVALID_USER);

            return WEB_ERROR;
        }

        // Check if remote ip is valid
        bool bValidHost;

        pthread_mutex_lock(&gpobj->m_mutex_UserList);
        bValidHost =
          (1 == pUserItem->isAllowedToConnect(inet_addr(reqinfo->remote_addr)));
        pthread_mutex_unlock(&gpobj->m_mutex_UserList);
        if (!bValidHost) {

            std::string strErr = vscp_str_format(
              "[REST Client] Host [%s] NOT allowed to connect. User [%s]",
              reqinfo->remote_addr,
              (const char*)keypairs["VSCPUSER"].c_str());

            syslog(LOG_ERR, "%s", strErr.c_str());

            restsrv_error(conn,
                          pSession,
                          format,
                          REST_ERROR_CODE_INVALID_ORIGIN);

            return WEB_ERROR;
        }

        // Is this an authorised user?
        pthread_mutex_lock(&gpobj->m_mutex_UserList);
        CUserItem* pValidUser =
          gpobj->m_userList.validateUser(keypairs["VSCPUSER"],
                                         keypairs["VSCPSECRET"]);
        pthread_mutex_unlock(&gpobj->m_mutex_UserList);

        if (NULL == pValidUser) {

            std::string strErr = vscp_str_format(
              "[REST Client] User [%s] NOT allowed to connect. Client [%s]",
              (const char*)keypairs["VSCPUSER"].c_str(),
              reqinfo->remote_addr);

            syslog(LOG_ERR, "%s", strErr.c_str());

            restsrv_error(conn,
                          pSession,
                          format,
                          REST_ERROR_CODE_INVALID_PASSWORD);

            return WEB_ERROR;
        }

        if (NULL == (pSession = restsrv_add_session(conn, pUserItem))) {

            // Hm,,, did not work out well...

            std::string strErr = vscp_str_format(
              ("[REST Client] Unable to create new session for user [%s]\n"),
              (const char*)keypairs[("VSCPUSER")].c_str());

            syslog(LOG_ERR, "%s", strErr.c_str());

            restsrv_error(conn,
                          pSession,
                          format,
                          REST_ERROR_CODE_INVALID_ORIGIN);

            return WEB_ERROR;
        }

        // Only the "open" command is allowed here
        if (("1" == keypairs["OP"]) || ("OPEN" == keypairs["OP"])) {

            if (__VSCP_DEBUG_REST) {
                syslog(LOG_DEBUG, "REST: restapi - doOpen format=%ld", format);
            }

            restsrv_doOpen(conn, pSession, format);

            return WEB_OK;
        }

        // !!! No meaning to go further - end it here !!!

        std::string strErr = vscp_str_format(
          "[REST Client] Unable to create new session for user [%s]",
          (const char*)keypairs[("VSCPUSER")].c_str());

        syslog(LOG_ERR, "%s", strErr.c_str());

        restsrv_error(conn, pSession, format, REST_ERROR_CODE_INVALID_ORIGIN);

        return WEB_ERROR;
    }

    // Check if remote ip is valid
    bool bValidHost;
    pthread_mutex_lock(&gpobj->m_mutex_UserList);
    bValidHost = (1 == pSession->m_pClientItem->m_pUserItem->isAllowedToConnect(
                         inet_addr(reqinfo->remote_addr)));
    pthread_mutex_unlock(&gpobj->m_mutex_UserList);
    if (!bValidHost) {

        std::string strErr = vscp_str_format(
          ("[REST Client] Host [%s] NOT allowed to connect. User [%s]\n"),
          std::string(reqinfo->remote_addr).c_str(),
          (const char*)keypairs[("VSCPUSER")].c_str());

        syslog(LOG_ERR, "%s", strErr.c_str());

        restsrv_error(conn, pSession, format, REST_ERROR_CODE_INVALID_ORIGIN);

        return WEB_ERROR;
    }

    // ------------------------------------------------------------------------
    //                  * * * User is validated * * *
    // ------------------------------------------------------------------------

    std::string strErr = vscp_str_format(
      ("[REST Client] User [%s] Host [%s] allowed to connect. \n"),
      (const char*)keypairs[("VSCPUSER")].c_str(),
      std::string(reqinfo->remote_addr).c_str());
    syslog(LOG_DEBUG, "%s", strErr.c_str());

    //   *************************************************************
    //   * * * * * * * *  Status (hold session open)   * * * * * * * *
    //   *************************************************************
    if ((("0") == keypairs[("OP")]) || (("STATUS") == keypairs[("OP")])) {
        try {
            restsrv_doStatus(conn, pSession, format);
        } catch (...) {
            syslog(LOG_ERR, "REST: Exception occurred doing restsrv_doStatus");
        }
    }

    //  ********************************************
    //  * * * * * * * * open session * * * * * * * *
    //  ********************************************
    else if ((("1") == keypairs[("OP")]) || (("OPEN") == keypairs[("OP")])) {
        try {
            restsrv_doOpen(conn, pSession, format);
        } catch (...) {
            syslog(LOG_ERR, "REST: Exception occurred doing restsrv_doOpen");
        }

    }

    //   **********************************************
    //   * * * * * * * * close session  * * * * * * * *
    //   **********************************************
    else if ((("2") == keypairs[("OP")]) || (("CLOSE") == keypairs[("OP")])) {
        try {
            restsrv_doClose(conn, pSession, format);
        } catch (...) {
            syslog(LOG_ERR, "REST: Exception occurred doing restsrv_doClose");
        }
    }

    //  ********************************************
    //   * * * * * * * * Send event  * * * * * * * *
    //  ********************************************
    else if ((("3") == keypairs[("OP")]) ||
             (("SENDEVENT") == keypairs[("OP")])) {
        vscpEvent vscpevent;
        if (("") != keypairs[("VSCPEVENT")]) {
            try {
                vscp_convertStringToEvent(&vscpevent, keypairs[("VSCPEVENT")]);
                restsrv_doSendEvent(conn, pSession, format, &vscpevent);
            } catch (...) {
                syslog(LOG_ERR,
                       "REST: Exception occurred doing restsrv_doSendEvent");
            }
        } else {
            // Parameter missing - No Event
            restsrv_error(conn, pSession, format, REST_ERROR_CODE_MISSING_DATA);
        }
    }

    //  ********************************************
    //   * * * * * * * * Read event  * * * * * * * *
    //  ********************************************
    else if ((("4") == keypairs[("OP")]) ||
             (("READEVENT") == keypairs[("OP")])) {
        long count = 1;
        if (("") != keypairs[("COUNT")]) {
            count = std::stoul(keypairs["COUNT"]);
        }
        try {
            restsrv_doReceiveEvent(conn, pSession, format, count);
        } catch (...) {
            syslog(LOG_ERR,
                   "REST: Exception occurred doing restsrv_doReceiveEvent");
        }
    }

    //   **************************************************
    //   * * * * * * * *     Set filter    * * * * * * * *
    //   **************************************************
    else if ((("5") == keypairs[("OP")]) ||
             (("SETFILTER") == keypairs[("OP")])) {

        vscpEventFilter vscpfilter;
        vscp_clearVSCPFilter(&vscpfilter);

        if (("") != keypairs[("VSCPFILTER")]) {
            vscp_readFilterFromString(&vscpfilter, keypairs[("VSCPFILTER")]);
        } else {
            restsrv_error(conn, pSession, format, REST_ERROR_CODE_MISSING_DATA);
        }

        if (("") != keypairs[("VSCPMASK")]) {
            vscp_readMaskFromString(&vscpfilter, keypairs[("VSCPMASK")]);
        } else {
            restsrv_error(conn, pSession, format, REST_ERROR_CODE_MISSING_DATA);
        }

        try {
            restsrv_doSetFilter(conn, pSession, format, vscpfilter);
        } catch (...) {
            syslog(LOG_ERR,
                   "REST: Exception occurred doing restsrv_doSetFilter");
        }

    }

    //   ****************************************************
    //   * * * * * * * *  clear input queue   * * * * * * * *
    //   ****************************************************
    else if ((("6") == keypairs[("OP")]) ||
             (("CLEARQUEUE") == keypairs[("OP")])) {
        try {
            restsrv_doClearQueue(conn, pSession, format);
        } catch (...) {
            syslog(LOG_ERR,
                   "REST: Exception occurred doing restsrv_doClearQueue");
        }
    }

    //   *************************************************
    //   * * * * * * * * Send measurement  * * * * * * * *
    //   *************************************************
    //   value,unit=0,sensor=0
    //
    else if ((("10") == keypairs[("OP")]) ||
             (("MEASUREMENT") == keypairs[("OP")])) {

        if ((("") != keypairs[("VALUE")]) && (("") != keypairs[("TYPE")])) {

            try {
                restsrv_doWriteMeasurement(conn,
                                           pSession,
                                           format,
                                           keypairs[("DATETIME")],
                                           keypairs[("GUID")],
                                           keypairs[("LEVEL")],
                                           keypairs[("TYPE")],
                                           keypairs[("VALUE")],
                                           keypairs[("UNIT")],
                                           keypairs[("SENSORIDX")],
                                           keypairs[("ZONE")],
                                           keypairs[("SUBZONE")],
                                           keypairs[("SUBZONE")]);
            } catch (...) {
                syslog(
                  LOG_ERR,
                  "REST: Exception occurred doing restsrv_doWriteMeasurement");
            }
        } else {
            restsrv_error(conn, pSession, format, REST_ERROR_CODE_MISSING_DATA);
        }
    }

    //   *******************************************
    //   * * * * * * * * Fetch MDF  * * * * * * * *
    //   *******************************************
    else if ((("12") == keypairs[("OP")]) || (("MDF") == keypairs[("OP")])) {

        if (("") != keypairs[("URL")]) {
            try {
                restsrv_doFetchMDF(conn, pSession, format, keypairs[("URL")]);
            } catch (...) {
                syslog(LOG_ERR,
                       "REST: Exception occurred doing restsrv_doFetchMDF");
            }
        } else {
            restsrv_error(conn, pSession, format, REST_ERROR_CODE_MISSING_DATA);
        }
    }

    // Unrecognised operation

    else {
        syslog(LOG_ERR, "REST: restapi - Missing data.");
        restsrv_error(conn, pSession, format, REST_ERROR_CODE_MISSING_DATA);
    }

    return WEB_OK;
}

///////////////////////////////////////////////////////////////////////////////
// restsrv_doOpen
//

void
restsrv_doOpen(struct mg_connection* conn,
               struct restsrv_session* pSession,
               int format)
{
    char wrkbuf[256];

    if (NULL != pSession) {

        // OK session

        // Note activity
        pSession->m_lastActiveTime = time(NULL);

        // Mark interface as open
        pSession->m_pClientItem->m_bOpen = true;

        if (REST_FORMAT_PLAIN == format) {

            websrv_sendSetCookieHeader(conn,
                                       200,
                                       REST_MIME_TYPE_PLAIN,
                                       pSession->m_sid);

#ifdef WIN32
            int n = _snprintf(wrkbuf,
                              sizeof(wrkbuf),
                              "1 1 Success vscpsession=%s nEvents=%zd",
                              pSession->sid,
                              pSession->pClientItem->m_clientInputQueue.size());
#else
            int n =
              snprintf(wrkbuf,
                       sizeof(wrkbuf),
                       "1 1 Success vscpsession=%s nEvents=%zu",
                       pSession->m_sid,
                       pSession->m_pClientItem->m_clientInputQueue.size());
#endif
            mg_write(conn, wrkbuf, n);
            mg_write(conn, "", 0); // Terminator
            return;
        } else if (REST_FORMAT_CSV == format) {

            websrv_sendSetCookieHeader(conn,
                                       200,
                                       REST_MIME_TYPE_CSV,
                                       pSession->m_sid);

#ifdef WIN32
            _snprintf(wrkbuf,
                      sizeof(wrkbuf),
                      "success-code,error-code,message,description,"
                      "vscpsession,nEvents\r\n1,1,Success,Success. 1,1"
                      ",Success,Success,%s,%zd",
                      pSession->sid,
                      pSession->pClientItem->m_clientInputQueue.size());
#else
            snprintf(wrkbuf,
                     sizeof(wrkbuf),
                     "success-code,error-code,message,description,"
                     "vscpsession,nEvents\r\n1,1,Success,Success. 1,1,"
                     "Success,Success,%s,%zu",
                     pSession->m_sid,
                     pSession->m_pClientItem->m_clientInputQueue.size());
#endif
            mg_write(conn, wrkbuf, strlen(wrkbuf));
            mg_write(conn, "", 0); // Terminator
            return;
        } else if (REST_FORMAT_XML == format) {

            websrv_sendSetCookieHeader(conn,
                                       200,
                                       REST_MIME_TYPE_XML,
                                       pSession->m_sid);

#ifdef WIN32
            nprintf(wrkbuf,
                    sizeof(wrkbuf),
                    "<vscp-rest success = \"true\" code = "
                    "\"1\" message = \"Success.\" description = "
                    "\"Success.\" ><vscpsession>%s</vscpsession>"
                    "<nEvents>%zd</nEvents></vscp-rest>",
                    pSession->sid,
                    pSession->pClientItem->m_clientInputQueue.size());
#else
            snprintf(wrkbuf,
                     sizeof(wrkbuf),
                     "<vscp-rest success = \"true\" code = \"1\" "
                     "message = \"Success.\" description = \"Success.\" "
                     "><vscpsession>%s</vscpsession><nEvents>%zu"
                     "</nEvents></vscp-rest>",
                     pSession->m_sid,
                     pSession->m_pClientItem->m_clientInputQueue.size());
#endif
            mg_write(conn, wrkbuf, strlen(wrkbuf));
            mg_write(conn, "", 0); // Terminator
            return;
        } else if (REST_FORMAT_JSON == format) {

            json output;

            websrv_sendSetCookieHeader(conn,
                                       200,
                                       REST_MIME_TYPE_JSON,
                                       pSession->m_sid);

            output["success"]     = true;
            output["code"]        = 1;
            output["message"]     = "success";
            output["description"] = "Success";
            output["vscpsession"] = pSession->m_sid;
            output["nEvents"] =
              pSession->m_pClientItem->m_clientInputQueue.size();
            std::string s = output.dump();
            mg_write(conn, s.c_str(), s.length());

            return;
        } else if (REST_FORMAT_JSONP == format) {

            json output;

            websrv_sendSetCookieHeader(conn,
                                       200,
                                       REST_MIME_TYPE_JSONP,
                                       pSession->m_sid);

            // Write JSONP start block
            mg_write(conn, REST_JSONP_START, strlen(REST_JSONP_START));

            output["success"]     = true;
            output["code"]        = 1;
            output["message"]     = "success";
            output["description"] = "Success";
            output["vscpsession"] = pSession->m_sid;
            output["nEvents"] =
              pSession->m_pClientItem->m_clientInputQueue.size();
            std::string s = output.dump();
            mg_write(conn, s.c_str(), s.length());

            // Write JSONP end block
            mg_write(conn, REST_JSONP_END, strlen(REST_JSONP_END));
            return;
        } else {
            websrv_sendheader(conn, 400, REST_MIME_TYPE_PLAIN);
            mg_write(conn,
                     REST_PLAIN_ERROR_UNSUPPORTED_FORMAT,
                     strlen(REST_PLAIN_ERROR_UNSUPPORTED_FORMAT));
            return;
        }
    } else { // Unable to create session
        restsrv_error(conn, pSession, format, REST_ERROR_CODE_INVALID_SESSION);
    }

    return;
}

///////////////////////////////////////////////////////////////////////////////
// restsrv_doClose
//

void
restsrv_doClose(struct mg_connection* conn,
                struct restsrv_session* pSession,
                int format)
{
    char wrkbuf[256];

    if (NULL != pSession) {

        char sid[32 + 1];
        memset(sid, 0, sizeof(sid));
        memcpy(sid, pSession->m_sid, sizeof(sid));

        // We should close the session

        // Mark as closed
        pSession->m_pClientItem->m_bOpen = false;

        // Note activity
        pSession->m_lastActiveTime = time(NULL);

        if (REST_FORMAT_PLAIN == format) {

            websrv_sendheader(conn, 200, REST_MIME_TYPE_PLAIN);
            mg_write(conn,
                     REST_PLAIN_ERROR_SUCCESS,
                     strlen(REST_PLAIN_ERROR_SUCCESS));
            mg_write(conn, "", 0); // Terminator
            return;

        } else if (REST_FORMAT_CSV == format) {

            websrv_sendheader(conn, 200, REST_MIME_TYPE_CSV);
#ifdef WIN32
            _snprintf(wrkbuf, sizeof(wrkbuf), REST_CSV_ERROR_SUCCESS);
#else
            snprintf(wrkbuf, sizeof(wrkbuf), REST_CSV_ERROR_SUCCESS);
#endif
            mg_write(conn, wrkbuf, strlen(wrkbuf));
            mg_write(conn, "", 0); // Terminator
            return;
        } else if (REST_FORMAT_XML == format) {

            websrv_sendheader(conn, 200, REST_MIME_TYPE_XML);
#ifdef WIN32
            _snprintf(wrkbuf, sizeof(wrkbuf), REST_XML_ERROR_SUCCESS);
#else
            snprintf(wrkbuf, sizeof(wrkbuf), REST_XML_ERROR_SUCCESS);
#endif
            mg_write(conn, wrkbuf, strlen(wrkbuf));
            mg_write(conn, "", 0); // Terminator
            return;
        } else if (REST_FORMAT_JSON == format) {

            websrv_sendheader(conn, 200, REST_MIME_TYPE_JSON);
#ifdef WIN32
            _snprintf(wrkbuf, sizeof(wrkbuf), REST_JSON_ERROR_SUCCESS);
#else
            snprintf(wrkbuf, sizeof(wrkbuf), REST_JSON_ERROR_SUCCESS);
#endif
            mg_write(conn, wrkbuf, strlen(wrkbuf));
            mg_write(conn, "", 0); // Terminator
            return;
        } else if (REST_FORMAT_JSONP == format) {

            websrv_sendheader(conn, 200, REST_MIME_TYPE_JSONP);
#ifdef WIN32
            _snprintf(wrkbuf, sizeof(wrkbuf), REST_JSONP_ERROR_SUCCESS);
#else
            snprintf(wrkbuf, sizeof(wrkbuf), REST_JSONP_ERROR_SUCCESS);
#endif
            mg_write(conn, wrkbuf, strlen(wrkbuf));
            mg_write(conn, "", 0); // Terminator
            return;
        } else {
            websrv_sendheader(conn, 400, REST_MIME_TYPE_PLAIN);
            mg_write(conn,
                     REST_PLAIN_ERROR_UNSUPPORTED_FORMAT,
                     strlen(REST_PLAIN_ERROR_UNSUPPORTED_FORMAT));
            mg_write(conn, "", 0); // Terminator
            return;
        }

    } else { // session not found
        restsrv_error(conn, pSession, format, REST_ERROR_CODE_INVALID_SESSION);
    }

    return;
}

///////////////////////////////////////////////////////////////////////////////
// restsrv_doStatus
//

void
restsrv_doStatus(struct mg_connection* conn,
                 struct restsrv_session* pSession,
                 int format)
{
    char buf[2048];
    char wrkbuf[256];

    if (NULL != pSession) {

        // Note activity
        pSession->m_lastActiveTime = time(NULL);

        if (REST_FORMAT_PLAIN == format) {
            websrv_sendheader(conn, 200, REST_MIME_TYPE_PLAIN);
            mg_write(conn,
                     REST_PLAIN_ERROR_SUCCESS,
                     strlen(REST_PLAIN_ERROR_SUCCESS));
            memset(buf, 0, sizeof(buf));
#ifdef WIN32
            _snprintf(wrkbuf,
                      sizeof(wrkbuf),
                      "vscpsession=%s nEvents=%zd",
                      pSession->sid,
                      pSession->pClientItem->m_clientInputQueue.size());
#else
            snprintf(wrkbuf,
                     sizeof(wrkbuf),
                     "1 1 Success vscpsession=%s nEvents=%zu",
                     pSession->m_sid,
                     pSession->m_pClientItem->m_clientInputQueue.size());
#endif
            mg_write(conn, wrkbuf, strlen(wrkbuf));
            mg_write(conn, "", 0); // Terminator
            return;
        } else if (REST_FORMAT_CSV == format) {
            websrv_sendheader(conn, 200, REST_MIME_TYPE_CSV);
#ifdef WIN32
            _snprintf(
              wrkbuf,
              sizeof(wrkbuf),
              "success-code,error-code,message,description,vscpsession,"
              "nEvents\r\n1,1,Success,Success. 1,1,Success,Sucess,%s,%zd",
              pSession->sid,
              pSession->pClientItem->m_clientInputQueue.size());
#else
            snprintf(
              wrkbuf,
              sizeof(wrkbuf),
              "success-code,error-code,message,description,vscpsession,"
              "nEvents\r\n1,1,Success,Success. 1,1,Success,Sucess,%s,%zu",
              pSession->m_sid,
              pSession->m_pClientItem->m_clientInputQueue.size());
#endif
            mg_write(conn, wrkbuf, strlen(wrkbuf));
            mg_write(conn, "", 0); // Terminator
            return;
        } else if (REST_FORMAT_XML == format) {
            websrv_sendheader(conn, 200, REST_MIME_TYPE_XML);
#ifdef WIN32
            _snprintf(wrkbuf,
                      sizeof(wrkbuf),
                      "<vscp-rest success = \"true\" code = \"1\" message = "
                      "\"Success.\" description = \"Success.\" "
                      "><vscpsession>%s</vscpsession><nEvents>%zd</nEvents></"
                      "vscp-rest>",
                      pSession->sid,
                      pSession->pClientItem->m_clientInputQueue.size());
#else
            snprintf(wrkbuf,
                     sizeof(wrkbuf),
                     "<vscp-rest success = \"true\" code = \"1\" message = "
                     "\"Success.\" description = \"Success.\" "
                     "><vscpsession>%s</vscpsession><nEvents>%zu</nEvents></"
                     "vscp-rest>",
                     pSession->m_sid,
                     pSession->m_pClientItem->m_clientInputQueue.size());
#endif
            mg_write(conn, wrkbuf, strlen(wrkbuf));
            mg_write(conn, "", 0); // Terminator
            return;
        } else if ((REST_FORMAT_JSON == format) ||
                   (REST_FORMAT_JSONP == format)) {

            json output;

            if (REST_FORMAT_JSON == format) {
                websrv_sendheader(conn, 200, REST_MIME_TYPE_JSON);
            } else {

                websrv_sendheader(conn, 200, REST_MIME_TYPE_JSONP);

                // Write JSONP start block
                mg_write(conn, REST_JSONP_START, strlen(REST_JSONP_START));
            }

            output["success"]     = true;
            output["code"]        = 1;
            output["message"]     = "success";
            output["description"] = "Success";
            output["vscpsession"] = pSession->m_sid;
            output["nEvents"] =
              pSession->m_pClientItem->m_clientInputQueue.size();
            std::string s = output.dump();
            mg_write(conn, s.c_str(), s.length());

            if (REST_FORMAT_JSONP == format) {
                // Write JSONP end block
                mg_write(conn, REST_JSONP_END, strlen(REST_JSONP_END));
            }

            return;
        } else {
            websrv_sendheader(conn, 400, REST_MIME_TYPE_PLAIN);
            mg_write(conn,
                     REST_PLAIN_ERROR_UNSUPPORTED_FORMAT,
                     strlen(REST_PLAIN_ERROR_UNSUPPORTED_FORMAT));
            mg_write(conn, "", 0); // Terminator
            return;
        }

    } // No session
    else {
        restsrv_error(conn, pSession, format, REST_ERROR_CODE_INVALID_SESSION);
    }

    return;
}

///////////////////////////////////////////////////////////////////////////////
// restsrv_doSendEvent
//

void
restsrv_doSendEvent(struct mg_connection* conn,
                    struct restsrv_session* pSession,
                    int format,
                    vscpEvent* pEvent)
{
    bool bSent = false;

    // Check pointer
    if (NULL == conn)
        return;

    if (NULL != pSession) {

        // Level II events between 512-1023 is recognised by the daemon and
        // sent to the correct interface as Level I events if the interface
        // is addressed by the client.
        if ((pEvent->vscp_class <= 1023) && (pEvent->vscp_class >= 512) &&
            (pEvent->sizeData >= 16)) {

            // This event should be sent to the correct interface if it is
            // available on this machine. If not it should be sent to
            // the rest of the network as normal

            cguid destguid;
            destguid.getFromArray(pEvent->pdata);
            destguid.setAt(0, 0);
            destguid.setAt(1, 0);

            if (NULL != pSession->m_pClientItem) {

                // Set client id
                pEvent->obid = pSession->m_pClientItem->m_clientID;

                // Check if filtered out
                if (vscp_doLevel2Filter(pEvent,
                                        &pSession->m_pClientItem->m_filter)) {

                    // Lock client
                    pthread_mutex_lock(&gpobj->m_clientList.m_mutexItemList);

                    // If the client queue is full for this client then the
                    // client will not receive the message
                    if (pSession->m_pClientItem->m_clientInputQueue.size() <=
                        gpobj->m_maxItemsInClientReceiveQueue) {

                        vscpEvent* pNewEvent = new (vscpEvent);
                        if (NULL != pNewEvent) {

                            vscp_copyEvent(pNewEvent, pEvent);

                            // Add the new event to the input queue
                            pthread_mutex_lock(&pSession->m_pClientItem
                                                  ->m_mutexClientInputQueue);
                            pSession->m_pClientItem->m_clientInputQueue
                              .push_back(pNewEvent);
                            pthread_mutex_unlock(&pSession->m_pClientItem
                                                    ->m_mutexClientInputQueue);
                            sem_post(
                              &pSession->m_pClientItem->m_semClientInputQueue);

                            bSent = true;
                        } else {
                            bSent = false;
                        }

                    } else {
                        // Overrun - No room for event
                        // vscp_deleteEvent( pEvent );
                        bSent = true;
                    }

                    // Unlock client
                    pthread_mutex_unlock(&gpobj->m_clientList.m_mutexItemList);

                } // filter

            } // Client found
        }

        if (!bSent) {

            if (NULL != pSession->m_pClientItem) {

                // Set client id
                pEvent->obid = pSession->m_pClientItem->m_clientID;

                // There must be room in the send queue
                if (gpobj->m_maxItemsInClientReceiveQueue >
                    gpobj->m_clientOutputQueue.size()) {

                    vscpEvent* pNewEvent = new (vscpEvent);
                    if (NULL != pNewEvent) {
                        vscp_copyEvent(pNewEvent, pEvent);

                        pthread_mutex_lock(&gpobj->m_mutex_ClientOutputQueue);
                        gpobj->m_clientOutputQueue.push_back(pNewEvent);
                        pthread_mutex_unlock(&gpobj->m_mutex_ClientOutputQueue);
                        sem_post(&gpobj->m_semClientOutputQueue);

                        bSent = true;
                    } else {
                        bSent = false;
                    }

                    restsrv_error(conn,
                                  pSession,
                                  format,
                                  REST_ERROR_CODE_SUCCESS);

                } else {
                    restsrv_error(conn,
                                  pSession,
                                  format,
                                  REST_ERROR_CODE_NO_ROOM);
                    vscp_deleteEvent(pEvent);
                    bSent = false;
                }

            } else {
                restsrv_error(conn,
                              pSession,
                              format,
                              REST_ERROR_CODE_GENERAL_FAILURE);
                vscp_deleteEvent(pEvent);
                bSent = false;
            }

        } // not sent
    } else {
        restsrv_error(conn, pSession, format, REST_ERROR_CODE_INVALID_SESSION);
        bSent = false;
    }

    return;
}

///////////////////////////////////////////////////////////////////////////////
// restsrv_doReceiveEvent
//

void
restsrv_doReceiveEvent(struct mg_connection* conn,
                       struct restsrv_session* pSession,
                       int format,
                       size_t count)
{
    // Check pointer
    if (NULL == conn)
        return;

    if (NULL != pSession) {

        if (!pSession->m_pClientItem->m_clientInputQueue.empty()) {

            char wrkbuf[32000];
            std::string out;
            size_t cntAvailable =
              pSession->m_pClientItem->m_clientInputQueue.size();

            // Plain
            if (REST_FORMAT_PLAIN == format) {

                // Send header
                websrv_sendheader(conn, 200, REST_MIME_TYPE_PLAIN);

                if (pSession->m_pClientItem->m_bOpen && cntAvailable) {

                    sprintf(wrkbuf, "1 1 Success \r\n");
                    mg_write(conn, wrkbuf, strlen(wrkbuf));
                    sprintf(wrkbuf,
#if WIN32
                            "%zd events requested of %zd available "
                            "(unfiltered) %zu will be retrieved\r\n",
#else
                            "%zd events requested of %zd available "
                            "(unfiltered) %zu will be retrieved\r\n",
#endif
                            count,
                            pSession->m_pClientItem->m_clientInputQueue.size(),
                            std::min(count, cntAvailable));

                    mg_write(conn, wrkbuf, strlen(wrkbuf));

                    for (unsigned int i = 0; i < std::min(count, cntAvailable);
                         i++) {

                        vscpEvent* pEvent;

                        pthread_mutex_lock(
                          &pSession->m_pClientItem->m_mutexClientInputQueue);
                        pEvent =
                          pSession->m_pClientItem->m_clientInputQueue.front();
                        pSession->m_pClientItem->m_clientInputQueue.pop_front();
                        pthread_mutex_unlock(
                          &pSession->m_pClientItem->m_mutexClientInputQueue);

                        if (NULL != pEvent) {

                            if (vscp_doLevel2Filter(
                                  pEvent,
                                  &pSession->m_pClientItem->m_filter)) {

                                std::string str;
                                if (vscp_convertEventToString(str, pEvent)) {

                                    // Write it out
                                    strcpy((char*)wrkbuf, (const char*)"- ");
                                    strcat((char*)wrkbuf,
                                           (const char*)str.c_str());
                                    strcat((char*)wrkbuf, "\r\n");
                                    mg_write(conn, wrkbuf, strlen(wrkbuf));

                                } else {
                                    strcpy(
                                      (char*)wrkbuf,
                                      "- Malformed event (internal error)\r\n");
                                    mg_write(conn, wrkbuf, strlen(wrkbuf));
                                }
                            } else {
                                strcpy((char*)wrkbuf,
                                       "- Event filtered out\r\n");
                                mg_write(conn, wrkbuf, strlen(wrkbuf));
                            }

                            // Remove the event
                            vscp_deleteEvent(pEvent);

                        } // Valid pEvent pointer
                        else {
                            strcpy((char*)wrkbuf,
                                   "- Event could not be fetched (internal "
                                   "error)\r\n");
                            mg_write(conn, wrkbuf, strlen(wrkbuf));
                        }
                    }    // for
                } else { // no events available
                    sprintf(wrkbuf, REST_PLAIN_ERROR_INPUT_QUEUE_EMPTY "\r\n");
                    mg_write(conn, wrkbuf, strlen(wrkbuf));
                }

            }

            // CSV
            else if (REST_FORMAT_CSV == format) {

                // Send header
                websrv_sendheader(conn,
                                  200,
                                  /*REST_MIME_TYPE_CSV*/ REST_MIME_TYPE_PLAIN);

                if (pSession->m_pClientItem->m_bOpen && cntAvailable) {

                    sprintf(wrkbuf,
                            "success-code,error-code,message,"
                            "description,Event\r\n1,1,Success,Success."
                            ",NULL\r\n");
                    mg_write(conn, wrkbuf, strlen(wrkbuf));
                    sprintf(wrkbuf,
#if WIN32
                            "1,2,Info,%zd events requested of %d available "
                            "(unfiltered) %zu will be retrieved,NULL\r\n",
#else
                            "1,2,Info,%zd events requested of %lu available "
                            "(unfiltered) %lu will be retrieved,NULL\r\n",
#endif
                            count,
                            (unsigned long)cntAvailable,
                            (unsigned long)std::min(count, cntAvailable));
                    mg_write(conn, wrkbuf, strlen(wrkbuf));
                    sprintf(wrkbuf,
                            "1,4,Count,%zu,NULL\r\n",
                            std::min(count, cntAvailable));
                    mg_write(conn, wrkbuf, strlen(wrkbuf));

                    for (unsigned int i = 0; i < std::min(count, cntAvailable);
                         i++) {

                        vscpEvent* pEvent;

                        pthread_mutex_lock(
                          &pSession->m_pClientItem->m_mutexClientInputQueue);
                        pEvent =
                          pSession->m_pClientItem->m_clientInputQueue.front();
                        pSession->m_pClientItem->m_clientInputQueue.pop_front();
                        pthread_mutex_unlock(
                          &pSession->m_pClientItem->m_mutexClientInputQueue);

                        if (NULL != pEvent) {

                            if (vscp_doLevel2Filter(
                                  pEvent,
                                  &pSession->m_pClientItem->m_filter)) {

                                std::string str;
                                if (vscp_convertEventToString(str, pEvent)) {

                                    // Write it out
                                    memset((char*)wrkbuf, 0, sizeof(wrkbuf));
                                    strcpy((char*)wrkbuf,
                                           (const char*)"1,3,Data,Event,");
                                    strcat((char*)wrkbuf,
                                           (const char*)str.c_str());
                                    strcat((char*)wrkbuf, "\r\n");
                                    mg_write(conn, wrkbuf, strlen(wrkbuf));

                                } else {
                                    strcpy((char*)wrkbuf,
                                           "1,2,Info,Malformed event (internal "
                                           "error)\r\n");
                                    mg_write(conn, wrkbuf, strlen(wrkbuf));
                                }
                            } else {
                                strcpy((char*)wrkbuf,
                                       "1,2,Info,Event filtered out\r\n");
                                mg_write(conn, wrkbuf, strlen(wrkbuf));
                            }

                            // Remove the event
                            vscp_deleteEvent(pEvent);

                        } // Valid pEvent pointer
                        else {
                            strcpy((char*)wrkbuf,
                                   "1,2,Info,Event could not be fetched "
                                   "(internal error)\r\n");
                            mg_write(conn, wrkbuf, strlen(wrkbuf));
                        }
                    }    // for
                } else { // no events available
                    sprintf(wrkbuf, REST_CSV_ERROR_INPUT_QUEUE_EMPTY "\r\n");
                    mg_write(conn, wrkbuf, strlen(wrkbuf));
                }

            }

            // XML
            else if (REST_FORMAT_XML == format) {

                int filtered = 0;
                int errors   = 0;

                // Send header
                websrv_sendheader(conn, 200, REST_MIME_TYPE_XML);

                if (pSession->m_pClientItem->m_bOpen && cntAvailable) {

                    sprintf(wrkbuf,
                            XML_HEADER "<vscp-rest success = \"true\" "
                                       "code = \"1\" message = \"Success\" "
                                       "description = \"Success.\" >");
                    mg_write(conn, wrkbuf, strlen(wrkbuf));
                    sprintf(wrkbuf,
                            "<info>%zd events requested of %zu available "
                            "(unfiltered) %zu will be retrieved</info>",
                            count,
                            cntAvailable,
                            std::min(count, cntAvailable));
                    mg_write(conn, wrkbuf, strlen(wrkbuf));

                    sprintf(wrkbuf,
                            "<count>%zu</count>",
                            std::min(count, cntAvailable));
                    mg_write(conn, wrkbuf, strlen(wrkbuf));

                    for (unsigned int i = 0;
                         i < std::min(count, cntAvailable);
                         i++) {

                        vscpEvent* pEvent;

                        pthread_mutex_lock(
                          &pSession->m_pClientItem->m_mutexClientInputQueue);
                        pEvent =
                          pSession->m_pClientItem->m_clientInputQueue.front();
                        pSession->m_pClientItem->m_clientInputQueue.pop_front();
                        pthread_mutex_unlock(
                          &pSession->m_pClientItem->m_mutexClientInputQueue);

                        if (NULL != pEvent) {

                            if (vscp_doLevel2Filter(
                                  pEvent,
                                  &pSession->m_pClientItem->m_filter)) {

                                std::string str;

                                // Write it out
                                strcpy((char*)wrkbuf, (const char*)"<event>");

                                // head
                                strcat((char*)wrkbuf, (const char*)"<head>");
                                strcat((char*)wrkbuf,
                                       vscp_str_format(("%d"), pEvent->head)
                                         .c_str());
                                strcat((char*)wrkbuf, (const char*)"</head>");

                                // class
                                strcat((char*)wrkbuf,
                                       (const char*)"<vscpclass>");
                                strcat(
                                  (char*)wrkbuf,
                                  vscp_str_format(("%d"), pEvent->vscp_class)
                                    .c_str());
                                strcat((char*)wrkbuf,
                                       (const char*)"</vscpclass>");

                                // type
                                strcat((char*)wrkbuf,
                                       (const char*)"<vscptype>");
                                strcat(
                                  (char*)wrkbuf,
                                  vscp_str_format(("%d"), pEvent->vscp_type)
                                    .c_str());
                                strcat((char*)wrkbuf,
                                       (const char*)"</vscptype>");

                                // obid
                                strcat((char*)wrkbuf, (const char*)"<obid>");
                                strcat((char*)wrkbuf,
                                       vscp_str_format(("%lu"), pEvent->obid)
                                         .c_str());
                                strcat((char*)wrkbuf, (const char*)"</obid>");

                                // datetime
                                strcat((char*)wrkbuf,
                                       (const char*)"<datetime>");
                                std::string dt;
                                vscp_getDateStringFromEvent(dt, pEvent);
                                strcat(
                                  (char*)wrkbuf,
                                  vscp_str_format("%s", dt.c_str()).c_str());
                                strcat((char*)wrkbuf,
                                       (const char*)"</datetime>");

                                // timestamp
                                strcat((char*)wrkbuf,
                                       (const char*)"<timestamp>");
                                strcat(
                                  (char*)wrkbuf,
                                  vscp_str_format(("%lu"), pEvent->timestamp)
                                    .c_str());
                                strcat((char*)wrkbuf,
                                       (const char*)"</timestamp>");

                                // guid
                                strcat((char*)wrkbuf, (const char*)"<guid>");
                                vscp_writeGuidToString(str, pEvent);
                                strcat((char*)wrkbuf, (const char*)str.c_str());
                                strcat((char*)wrkbuf, (const char*)"</guid>");

                                // sizedate
                                strcat((char*)wrkbuf,
                                       (const char*)"<sizedata>");
                                strcat((char*)wrkbuf,
                                       vscp_str_format(("%d"), pEvent->sizeData)
                                         .c_str());
                                strcat((char*)wrkbuf,
                                       (const char*)"</sizedata>");

                                // data
                                strcat((char*)wrkbuf, (const char*)"<data>");
                                vscp_writeDataToString(str, pEvent);
                                strcat((char*)wrkbuf, (const char*)str.c_str());
                                strcat((char*)wrkbuf, (const char*)"</data>");

                                if (vscp_convertEventToString(str, pEvent)) {
                                    strcat((char*)wrkbuf, (const char*)"<raw>");
                                    strcat((char*)wrkbuf,
                                           (const char*)str.c_str());
                                    strcat((char*)wrkbuf,
                                           (const char*)"</raw>");
                                }

                                strcat((char*)wrkbuf, "</event>");
                                mg_write(conn, wrkbuf, strlen(wrkbuf));

                            } else {
                                filtered++;
                            }

                            // Remove the event
                            vscp_deleteEvent(pEvent);

                        } // Valid pEvent pointer
                        else {
                            errors++;
                        }
                    } // for

                    strcpy((char*)wrkbuf, (const char*)"<filtered>");
                    strcat((char*)wrkbuf,
                           vscp_str_format(("%d"), filtered).c_str());
                    strcat((char*)wrkbuf, (const char*)"</filtered>");

                    strcat((char*)wrkbuf, (const char*)"<errors>");
                    strcat((char*)wrkbuf,
                           vscp_str_format(("%d"), errors).c_str());
                    strcat((char*)wrkbuf, (const char*)"</errors>");

                    mg_write(conn, wrkbuf, strlen(wrkbuf));

                    // End tag
                    strcpy((char*)wrkbuf, "</vscp-rest>");
                    mg_write(conn, wrkbuf, strlen(wrkbuf));

                } else { // no events available

                    sprintf(wrkbuf, REST_XML_ERROR_INPUT_QUEUE_EMPTY "\r\n");
                    mg_write(conn, wrkbuf, strlen(wrkbuf));
                }

            }

            // JSON / JSONP
            else if ((REST_FORMAT_JSON == format) ||
                     (REST_FORMAT_JSONP == format)) {

                int sentEvents = 0;
                int filtered   = 0;
                int errors     = 0;
                json output;

                // Send header
                if (REST_FORMAT_JSONP == format) {
                    websrv_sendheader(conn, 200, REST_MIME_TYPE_JSONP);
                } else {
                    websrv_sendheader(conn, 200, REST_MIME_TYPE_JSON);
                }

                if (pSession->m_pClientItem->m_bOpen && cntAvailable) {

                    // typeof handler === 'function' &&
                    if (REST_FORMAT_JSONP == format) {
                        std::string str =
                          ("typeof handler === 'function' && handler(");
                        mg_write(conn, (const char*)str.c_str(), str.length());
                    }

                    output["success"]     = true;
                    output["code"]        = 1;
                    output["message"]     = "success";
                    output["description"] = "Success";
                    output["info"]        = (const char*)vscp_str_format(
                                       "%zd events requested of %lu available "
                                       "(unfiltered) %zd will be retrieved",
                                       count,
                                       cntAvailable,
                                       std::min(count, cntAvailable))
                                       .c_str();

                    for (unsigned int i = 0; i < std::min(count, cntAvailable);
                         i++) {

                        vscpEvent* pEvent;

                        pthread_mutex_lock(
                          &pSession->m_pClientItem->m_mutexClientInputQueue);
                        pEvent =
                          pSession->m_pClientItem->m_clientInputQueue.front();
                        pSession->m_pClientItem->m_clientInputQueue.pop_front();
                        pthread_mutex_unlock(
                          &pSession->m_pClientItem->m_mutexClientInputQueue);

                        if (NULL != pEvent) {

                            if (vscp_doLevel2Filter(
                                  pEvent,
                                  &pSession->m_pClientItem->m_filter)) {

                                std::string str;
                                json ev;
                                ev["head"]      = pEvent->head;
                                ev["vscpclass"] = pEvent->vscp_class;
                                ev["vscptype"]  = pEvent->vscp_type;
                                vscp_getDateStringFromEvent(str, pEvent);
                                ev["datetime"]  = (const char*)str.c_str();
                                ev["timestamp"] = pEvent->timestamp;
                                ev["obid"]      = pEvent->obid;
                                vscp_writeGuidToString(str, pEvent);
                                ev["guid"]     = (const char*)str.c_str();
                                ev["sizedata"] = pEvent->sizeData;
                                ev["data"]     = json::array();
                                for (uint16_t j = 0; j < pEvent->sizeData;
                                     j++) {
                                    ev["data"].push_back(pEvent->pdata[j]);
                                }

                                // Add event to event array
                                output["event"].push_back(ev);

                                sentEvents++;

                            } else {
                                filtered++;
                            }

                            // Remove the event
                            vscp_deleteEvent(pEvent);

                        } // Valid pEvent pointer
                        else {
                            errors++;
                        }

                    } // for

                    // Mark end
                    output["count"]    = sentEvents;
                    output["filtered"] = filtered;
                    output["errors"]   = errors;

                    std::string s = output.dump();
                    mg_write(conn, s.c_str(), s.length());

                    if (REST_FORMAT_JSONP == format) {
                        mg_write(conn, ");", 2);
                    }

                }      // if open and data
                else { // no events available

                    if (REST_FORMAT_JSON == format) {
                        sprintf(wrkbuf,
                                REST_JSON_ERROR_INPUT_QUEUE_EMPTY "\r\n");
                    } else {
                        sprintf(wrkbuf,
                                REST_JSONP_ERROR_INPUT_QUEUE_EMPTY "\r\n");
                    }

                    mg_write(conn, wrkbuf, strlen(wrkbuf));
                }

            } // format

            mg_write(conn, "", 0);

        } else { // Queue is empty
            restsrv_error(conn,
                          pSession,
                          format,
                          RESR_ERROR_CODE_INPUT_QUEUE_EMPTY);
        }

    } else {
        restsrv_error(conn, pSession, format, REST_ERROR_CODE_INVALID_SESSION);
    }

    return;
}

///////////////////////////////////////////////////////////////////////////////
// restsrv_doSetFilter
//

void
restsrv_doSetFilter(struct mg_connection* conn,
                    struct restsrv_session* pSession,
                    int format,
                    vscpEventFilter& vscpfilter)
{
    if (NULL != pSession) {

        pthread_mutex_lock(&pSession->m_pClientItem->m_mutexClientInputQueue);
        memcpy(&pSession->m_pClientItem->m_filter,
               &vscpfilter,
               sizeof(vscpEventFilter));
        pthread_mutex_unlock(&pSession->m_pClientItem->m_mutexClientInputQueue);
        restsrv_error(conn, pSession, format, REST_ERROR_CODE_SUCCESS);
    } else {
        restsrv_error(conn, pSession, format, REST_ERROR_CODE_INVALID_SESSION);
    }

    return;
}

///////////////////////////////////////////////////////////////////////////////
// restsrv_doClearQueue
//

void
restsrv_doClearQueue(struct mg_connection* conn,
                     struct restsrv_session* pSession,
                     int format)
{
    // Check pointer
    if (NULL == conn)
        return;

    if (NULL != pSession) {

        std::deque<vscpEvent*>::iterator it;

        pthread_mutex_lock(&pSession->m_pClientItem->m_mutexClientInputQueue);
        for (it = pSession->m_pClientItem->m_clientInputQueue.begin();
             it != pSession->m_pClientItem->m_clientInputQueue.end();
             ++it) {
            vscpEvent* pEvent = *it;
            vscp_deleteEvent_v2(&pEvent);
        }
        pSession->m_pClientItem->m_clientInputQueue.clear();
        pthread_mutex_unlock(&pSession->m_pClientItem->m_mutexClientInputQueue);

        restsrv_error(conn, pSession, format, REST_ERROR_CODE_SUCCESS);
    } else {
        restsrv_error(conn, pSession, format, REST_ERROR_CODE_INVALID_SESSION);
    }

    return;
}

///////////////////////////////////////////////////////////////////////////////
// restsrv_doWriteMeasurement
//

void
restsrv_doWriteMeasurement(struct mg_connection* conn,
                           struct restsrv_session* pSession,
                           int format,
                           std::string& strDateTime,
                           std::string& strGuid,
                           std::string& strLevel,
                           std::string& strType,
                           std::string& strValue,
                           std::string& strUnit,
                           std::string& strSensorIdx,
                           std::string& strZone,
                           std::string& strSubZone,
                           std::string& strEventFormat)
{
    if (NULL != pSession) {

        double value = 0;
        uint8_t guid[16];
        long level = 2;
        long unit;
        long vscptype;
        long sensoridx;
        long zone;
        long subzone;
        long eventFormat = 0; // float
        uint8_t data[VSCP_MAX_DATA];
        uint16_t sizeData;

        memset(guid, 0, 16);
        vscp_getGuidFromStringToArray(guid, strGuid);

        value     = std::stod(strValue);                // Measurement value
        unit      = vscp_readStringValue(strUnit);      // Measurement unit
        sensoridx = vscp_readStringValue(strSensorIdx); // Sensor index
        vscptype  = vscp_readStringValue(strType);      // VSCP event type
        zone      = vscp_readStringValue(strZone);      // VSCP event type
        zone &= 0xff;
        subzone = vscp_readStringValue(strSubZone); // VSCP event type
        subzone &= 0xff;

        // datetime
        vscpdatetime dt;
        if (!dt.set(strDateTime)) {
            dt = vscpdatetime::UTCNow();
        }

        level = vscp_readStringValue(strLevel); // Level I or Level II (default)
        if ((level > 2) || (level < 1)) {
            level = 2;
        }

        vscp_trim(strEventFormat);
        if (0 != vscp_strcasecmp(strEventFormat.c_str(), "STRING")) {
            eventFormat = 1;
        }

        // Range checks
        if (1 == level) {
            if (unit > 3)
                unit = 0;
            if (sensoridx > 7)
                sensoridx = 0;
            if (vscptype > 512)
                vscptype -= 512;
        } else if (2 == level) {
            if (unit > 255)
                unit &= 0xff;
            if (sensoridx > 255)
                sensoridx &= 0xff;
        }

        if (1 == level) {

            if (0 == eventFormat) {

                // Floating point
                if (vscp_convertFloatToFloatEventData(data,
                                                      &sizeData,
                                                      value,
                                                      unit,
                                                      sensoridx)) {
                    if (sizeData > 8)
                        sizeData = 8;

                    vscpEvent* pEvent = new vscpEvent;
                    if (NULL == pEvent) {
                        restsrv_error(conn,
                                      pSession,
                                      format,
                                      REST_ERROR_CODE_GENERAL_FAILURE);
                        return;
                    }
                    pEvent->pdata = NULL;

                    pEvent->head      = VSCP_PRIORITY_NORMAL;
                    pEvent->timestamp = 0; // Let interface fill in
                    memcpy(pEvent->GUID, guid, 16);
                    pEvent->sizeData = sizeData;
                    if (sizeData > 0) {
                        pEvent->pdata = new uint8_t[sizeData];
                        memcpy(pEvent->pdata, data, sizeData);
                    }
                    pEvent->vscp_class = VSCP_CLASS1_MEASUREMENT;
                    pEvent->vscp_type  = vscptype;

                    restsrv_doSendEvent(conn, pSession, format, pEvent);

                } else {
                    restsrv_error(conn,
                                  pSession,
                                  format,
                                  REST_ERROR_CODE_GENERAL_FAILURE);
                }
            } else {

                // String
                vscpEvent* pEvent = new vscpEvent;
                if (NULL == pEvent) {
                    restsrv_error(conn,
                                  pSession,
                                  format,
                                  REST_ERROR_CODE_GENERAL_FAILURE);
                    return;
                }
                pEvent->pdata = NULL;

                if (vscp_makeStringMeasurementEvent(pEvent,
                                                    value,
                                                    unit,
                                                    sensoridx)) {

                } else {
                    restsrv_error(conn,
                                  pSession,
                                  format,
                                  REST_ERROR_CODE_GENERAL_FAILURE);
                }
            }
        } else { // Level II

            if (0 == eventFormat) {

                // Floating point
                vscpEvent* pEvent = new vscpEvent;
                if (NULL == pEvent) {
                    restsrv_error(conn,
                                  pSession,
                                  format,
                                  REST_ERROR_CODE_GENERAL_FAILURE);
                    return;
                }

                pEvent->pdata = NULL;

                pEvent->obid      = 0;
                pEvent->head      = VSCP_PRIORITY_NORMAL;
                pEvent->timestamp = 0; // Let interface fill in
                memcpy(pEvent->GUID, guid, 16);
                pEvent->head       = 0;
                pEvent->vscp_class = VSCP_CLASS2_MEASUREMENT_FLOAT;
                pEvent->vscp_type  = vscptype;
                pEvent->timestamp  = 0;
                pEvent->sizeData   = 12;

                data[0] = sensoridx;
                data[1] = zone;
                data[2] = subzone;
                data[3] = unit;

                memcpy(data + 4, (uint8_t*)&value, 8); // copy in double
                uint64_t temp = VSCP_UINT64_SWAP_ON_LE(*(data + 4));
                memcpy(data + 4, (void*)&temp, 8);

                // Copy in data
                pEvent->pdata = new uint8_t[4 + 8];
                if (NULL == pEvent->pdata) {
                    restsrv_error(conn,
                                  pSession,
                                  format,
                                  REST_ERROR_CODE_GENERAL_FAILURE);
                    delete pEvent;
                    return;
                }
                memcpy(pEvent->pdata, data, 4 + 8);

                restsrv_doSendEvent(conn, pSession, format, pEvent);
            } else {
                // String
                vscpEvent* pEvent = new vscpEvent;
                pEvent->pdata     = NULL;

                pEvent->obid      = 0;
                pEvent->head      = VSCP_PRIORITY_NORMAL;
                pEvent->timestamp = 0; // Let interface fill in
                memcpy(pEvent->GUID, guid, 16);
                pEvent->head       = 0;
                pEvent->vscp_class = VSCP_CLASS2_MEASUREMENT_STR;
                pEvent->vscp_type  = vscptype;
                pEvent->timestamp  = 0;
                pEvent->sizeData   = 12;

                data[0] = sensoridx;
                data[1] = zone;
                data[2] = subzone;
                data[3] = unit;

                pEvent->pdata = new uint8_t[4 + strValue.length()];
                if (NULL == pEvent->pdata) {
                    restsrv_error(conn,
                                  pSession,
                                  format,
                                  REST_ERROR_CODE_GENERAL_FAILURE);
                    delete pEvent;
                    return;
                }
                memcpy(data + 4,
                       strValue.c_str(),
                       strValue.length()); // copy in double

                restsrv_doSendEvent(conn, pSession, format, pEvent);
            }
        }

        restsrv_error(conn, pSession, format, REST_ERROR_CODE_SUCCESS);
    } else {
        restsrv_error(conn, pSession, format, REST_ERROR_CODE_INVALID_SESSION);
    }

    return;
}

///////////////////////////////////////////////////////////////////////////////
// restsrv_doFetchMDF
//

void
restsrv_doFetchMDF(struct mg_connection* conn,
                   struct restsrv_session* pSession,
                   int format,
                   std::string& strURL)
{

    CMDF mdf;

    if (mdf.load(strURL, false)) {

        // Loaded OK

        if (REST_FORMAT_PLAIN == format) {
            restsrv_error(conn,
                          pSession,
                          format,
                          REST_ERROR_CODE_GENERAL_FAILURE);
        } else if (REST_FORMAT_CSV == format) {
            restsrv_error(conn,
                          pSession,
                          format,
                          REST_ERROR_CODE_GENERAL_FAILURE);
        } else if (REST_FORMAT_XML == format) {

            // Send header
            websrv_sendheader(conn, 200, REST_MIME_TYPE_XML);

            std::string line;
            std::ifstream file(mdf.getTempFilePath());

            if (file.is_open()) {
                while (!file.fail() && !file.eof() && getline(file, line)) {
                    line += "\n";
                    mg_write(conn, line.c_str(), line.length());
                }
                file.close();
            }

            file.close();

            mg_write(conn, "", 0); // Terminator
        } else if ((REST_FORMAT_JSON == format) ||
                   (REST_FORMAT_JSONP == format)) {
            restsrv_error(conn,
                          pSession,
                          format,
                          REST_ERROR_CODE_GENERAL_FAILURE);
        }
    } else {
        // Failed to load
        restsrv_error(conn, pSession, format, REST_ERROR_CODE_GENERAL_FAILURE);
    }

    return;
}
