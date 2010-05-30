/*
 * Copyright (c) 2009 Vic Lee
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xdmcpclient.h"

#define XQPROXY_ERROR_ARGUMENT  1
#define XQPROXY_ERROR_INIT      2
#define XQPROXY_ERROR_RESOLVE   3
#define XQPROXY_ERROR_QUERY     4
#define XQPROXY_ERROR_REQUEST   5
#define XQPROXY_ERROR_XAUTH     6
#define XQPROXY_ERROR_MANAGE    7

int
main (int argc, char *argv[])
{
    XdmcpClient *client;
    int ret = 0;
    int i;
    int display = 0;
    unsigned int sessionid = 0;
    const char *host = "localhost", *port = "177";
    int do_query = 0;
    int do_xauth = 0;
    int do_manage = 0;
    int timeout = 5;

    for (i = 1; i < argc; i++)
    {
        if (strcmp (argv[i], "-display") == 0)
        {
            if (i + 1 >= argc)
            {
                printf ("Error> Missing argument value %s\n", argv[i]);
                return XQPROXY_ERROR_ARGUMENT;
            }
            display = atoi (argv[++i]);
            continue;
        }
        if (strcmp (argv[i], "-host") == 0)
        {
            if (i + 1 >= argc)
            {
                printf ("Error> Missing argument value %s\n", argv[i]);
                return XQPROXY_ERROR_ARGUMENT;
            }
            host = argv[++i];
            continue;
        }
        if (strcmp (argv[i], "-port") == 0)
        {
            if (i + 1 >= argc)
            {
                printf ("Error> Missing argument value %s\n", argv[i]);
                return XQPROXY_ERROR_ARGUMENT;
            }
            port = argv[++i];
            continue;
        }
        if (strcmp (argv[i], "-sessionid") == 0)
        {
            if (i + 1 >= argc)
            {
                printf ("Error> Missing argument value %s\n", argv[i]);
                return XQPROXY_ERROR_ARGUMENT;
            }
            sessionid = (unsigned int) atoi (argv[++i]);
            continue;
        }
        if (strcmp (argv[i], "-timeout") == 0)
        {
            if (i + 1 >= argc)
            {
                printf ("Error> Missing argument value %s\n", argv[i]);
                return XQPROXY_ERROR_ARGUMENT;
            }
            timeout = (unsigned int) atoi (argv[++i]);
            continue;
        }
        if (strcmp (argv[i], "-query") == 0)
        {
            do_query = 1;
            continue;
        }
        if (strcmp (argv[i], "-xauth") == 0)
        {
            do_xauth = 1;
            continue;
        }
        if (strcmp (argv[i], "-manage") == 0)
        {
            do_manage = 1;
            continue;
        }
    }

    if (do_xauth && !do_query)
    {
        printf ("Error> Argument -xauth must be used together with -query\n");
        return XQPROXY_ERROR_ARGUMENT;
    }

    if (!do_query && !do_manage)
    {
        printf ("Error> Either -query or -manage must be supplied\n");
        return XQPROXY_ERROR_ARGUMENT;
    }

    if (do_manage && !sessionid && !do_query)
    {
        printf ("Error> Argument -sessionid must be supplied if -manage is used without -query\n");
        return XQPROXY_ERROR_ARGUMENT;
    }

    if (sessionid && do_query)
    {
        printf ("Error> Argument -sessionid cannot be used together with -query\n");
        return XQPROXY_ERROR_ARGUMENT;
    }

    client = XdmcpClientNew (display, timeout);
    if (!client) return XQPROXY_ERROR_INIT;
    if (XdmcpClientRegisterServer (client, host, port))
    {
        if (do_query)
        {
            if (!XdmcpClientQuery (client)) ret = XQPROXY_ERROR_QUERY;
            else if (!XdmcpClientRequest (client)) ret = XQPROXY_ERROR_REQUEST;
            else if (do_xauth && !XdmcpClientAddXauth (client)) ret = XQPROXY_ERROR_XAUTH;
        }
        if (ret == 0 && do_manage)
        {
            if (!XdmcpClientManage (client, sessionid))
            {
                ret = XQPROXY_ERROR_MANAGE;
            }
        }
    }
    else
    {
        ret = XQPROXY_ERROR_RESOLVE;
    }
    XdmcpClientFree (client);
    return ret;
}
