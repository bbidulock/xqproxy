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
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <X11/Xdmcp.h>
#include "xdmcpclient.h"

static const char* HEX = "0123456789abcdef";

static void
PrintARRAY8 (ARRAY8Ptr arr, int ascii)
{
    int i;
    for (i = 0; i < arr->length; i++)
    {
        if (ascii)
        {
            putc (arr->data[i], stdout);
        }
        else
        {
            putc (HEX[arr->data[i] / 16 % 16], stdout);
            putc (HEX[arr->data[i] % 16], stdout);
        }
    }
}

static char*
ConvertARRAY8toString (ARRAY8Ptr arr, int ascii)
{
    char *s;
    int i;

    if (ascii)
    {
        s = (char*) malloc (arr->length + 1);
        for (i = 0; i < arr->length; i++)
        {
            s[i] = arr->data[i];
        }
        s[arr->length] = 0;
    }
    else
    {
        s = (char*) malloc (arr->length * 2 + 1);
        for (i = 0; i < arr->length; i++)
        {
            s[i * 2] = HEX[arr->data[i] / 16 % 16];
            s[i * 2 + 1] = HEX[arr->data[i] % 16];
        }
        s[arr->length * 2] = 0;
    }
    return s;
}

static int
ARRAY8ofARRAY8Len (ARRAYofARRAY8Ptr arr)
{
    int i;
    int len;
    len = 0;
    for (i = 0; i < arr->length; i++)
    {
        len += 2 + arr->data[i].length;
    }
    return len;
}

struct _XdmcpClient
{
    struct sockaddr_storage Addr;
    int AddrLen;
    int Sock;
    int Timeout;

    XdmcpHeader header;
    XdmcpBuffer buffer;

    ARRAYofARRAY8 AuthenNames;
    ARRAYofARRAY8 AuthenDatas;
    ARRAY8 AuthenName;
    ARRAY8 AuthenData;

    ARRAYofARRAY8 AuthorNames;
    ARRAY8 AuthorName;
    ARRAY8 AuthorData;

    int DisplayNumber;
    ARRAY8 DisplayClass;
    ARRAY8 DisplayID;

    CARD32 SessionID;
};

static void
XdmcpClientRegisterAuthen (XdmcpClient *client, const char *name, const char *data, int datalen)
{
    int i;
    int len;
    int namelen;
    ARRAY8 n, d;

    namelen = strlen (name);
    XdmcpAllocARRAY8 (&n, namelen);
    XdmcpAllocARRAY8 (&d, datalen);
    for (i = 0; i < namelen; i++) n.data[i] = name[i];
    for (i = 0; i < datalen; i++) d.data[i] = data[i];

    len = client->AuthenNames.length + 1;
    XdmcpReallocARRAYofARRAY8 (&client->AuthenNames, len);
    XdmcpReallocARRAYofARRAY8 (&client->AuthenDatas, len);
    client->AuthenNames.data[len - 1] = n;
    client->AuthenDatas.data[len - 1] = d; 
}

static void
XdmcpClientRegisterDisplayClass (XdmcpClient *client, const char *displayclass)
{
    int len;

    XdmcpDisposeARRAY8 (&client->DisplayClass);
    len = strlen (displayclass);
    XdmcpAllocARRAY8 (&client->DisplayClass, len);
    memcpy (client->DisplayClass.data, displayclass, len);
}

static void
XdmcpClientRegisterDisplayID (XdmcpClient *client, const char *displayid)
{
    int len;

    XdmcpDisposeARRAY8 (&client->DisplayID);
    len = strlen (displayid);
    XdmcpAllocARRAY8 (&client->DisplayID, len);
    memcpy (client->DisplayID.data, displayid, len);
}

static void
XdmcpClientRegisterAuthor (XdmcpClient *client, const char *name)
{
    int i;
    int len;
    int namelen;
    ARRAY8 n;

    namelen = strlen (name);
    XdmcpAllocARRAY8 (&n, namelen);
    for (i = 0; i < namelen; i++) n.data[i] = name[i];

    len = client->AuthorNames.length + 1;
    XdmcpReallocARRAYofARRAY8 (&client->AuthorNames, len);
    client->AuthorNames.data[len - 1] = n;
}

int
XdmcpClientRegisterServer (XdmcpClient *client, const char *host, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *resi;
    int error;
    int sock;

    memset (&hints, 0, sizeof (hints));
    hints.ai_family = PF_UNSPEC;
    error = getaddrinfo (host, port, &hints, &res);
    if (error)
    {
        printf ("Error> getaddrinfo on %s:%s failed: %s.\n", host, port, gai_strerror (error));
        return FALSE;
    }
    for (resi = res; resi; resi = resi->ai_next)
    {
        if (resi->ai_family == AF_INET || resi->ai_family == AF_INET6)
        {
            break;
        }
    }
    if (resi == NULL)
    {
        printf ("Error> host not on supported network type.\n");
        return FALSE;
    }

    memmove (&client->Addr, resi->ai_addr, resi->ai_addrlen);
    client->AddrLen = resi->ai_addrlen;

    sock = socket (resi->ai_family, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        printf ("Error> failed to create socket.\n");
        return FALSE;
    }
    client->Sock = sock;

    freeaddrinfo (res);

    printf ("Client> Server address %s:%s registered.\n", host, port);

    return TRUE;
}

XdmcpClient*
XdmcpClientNew (int display_number, int timeout)
{
    XdmcpClient *client;

    client = (XdmcpClient*) malloc (sizeof (XdmcpClient));
    memset (client, 0, sizeof (XdmcpClient));
    client->Sock = -1;
    client->Timeout = timeout;

    XdmcpClientRegisterAuthen (client, "XDM-AUTHENTICATION-1", "test", 4);
    XdmcpClientRegisterDisplayClass (client, "xqproxy");
    XdmcpClientRegisterDisplayID (client, "xqproxy");
    XdmcpClientRegisterAuthor (client, "MIT-MAGIC-COOKIE-1");

    client->DisplayNumber = display_number;

    printf ("Client> Initialized on display :%i.\n", display_number);

    return client;
}

void
XdmcpClientFree (XdmcpClient *client)
{
    if (client->Sock >= 0)
    {
        close (client->Sock);
    }
    XdmcpDisposeARRAYofARRAY8 (&client->AuthenNames);
    XdmcpDisposeARRAYofARRAY8 (&client->AuthenDatas);
    XdmcpDisposeARRAYofARRAY8 (&client->AuthorNames);
    XdmcpDisposeARRAY8 (&client->AuthorName);
    XdmcpDisposeARRAY8 (&client->AuthorData);
    XdmcpDisposeARRAY8 (&client->DisplayClass);
    XdmcpDisposeARRAY8 (&client->DisplayID);
    if (client->buffer.data)
        Xfree (client->buffer.data);
    free (client);
}

int
XdmcpClientReceivePacket (XdmcpClient *client)
{
    fd_set fsread;
    fd_set fserr;
    struct timeval timeout;
    int ret;

    FD_ZERO (&fsread);
    FD_SET (client->Sock, &fsread);
    FD_ZERO (&fserr);
    FD_SET (client->Sock, &fserr);
    timeout.tv_sec = client->Timeout;
    timeout.tv_usec = 0;

    ret = select(FD_SETSIZE, &fsread, NULL, &fserr, &timeout);
    if (ret < 0)
    {
        printf("Error> Failed to call select().\n");
        return FALSE;
    }
    if (FD_ISSET (client->Sock, &fserr))
    {
        printf("Error> Socket error.\n");
        return FALSE;
    }
    if (!FD_ISSET (client->Sock, &fsread))
    {
        printf ("Error> No response from the server.\n");
        return FALSE;
    }

    if (!XdmcpFill (client->Sock, &client->buffer, (XdmcpNetaddr) &client->Addr, &client->AddrLen))
    {
        printf ("Error> Failed to receive from the server.\n");
        return FALSE;
    }
    if (!XdmcpReadHeader (&client->buffer, &client->header))
    {
        printf ("Error> Received corrupted packet header.\n");
        return FALSE;
    }
    if (client->header.version != XDM_PROTOCOL_VERSION)
    {
        printf ("Error> Received unsupported version %i.\n", client->header.version);
        return FALSE;
    }
    return TRUE;
}

int
XdmcpClientSetAuth (XdmcpClient *client, ARRAY8Ptr authenname)
{
    int ret = FALSE;
    int i;
    if (authenname->length == 0)
    {
        ret = TRUE;
    }
    else
    {
        for (i = 0; i < client->AuthenNames.length; i++)
        {
            if (XdmcpARRAY8Equal (&client->AuthenNames.data[i], authenname))
            {
                client->AuthenName = client->AuthenNames.data[i];
                client->AuthenData = client->AuthenDatas.data[i];
                ret = TRUE;
                break;
            }
        }
    }
    return ret;
}

int
XdmcpClientQuery (XdmcpClient *client)
{
    int ret;
    ARRAY8 authenname = { 0 };
    ARRAY8 hostname = { 0 };
    ARRAY8 status = { 0 };

    client->header.version = XDM_PROTOCOL_VERSION;
    client->header.opcode = (CARD16) QUERY; 
    client->header.length = 1;
    client->header.length += ARRAY8ofARRAY8Len (&client->AuthenNames);
    XdmcpWriteHeader (&client->buffer, &client->header);
    XdmcpWriteARRAYofARRAY8 (&client->buffer, &client->AuthenNames);
    XdmcpFlush (client->Sock, &client->buffer, (XdmcpNetaddr) &client->Addr, client->AddrLen);

    printf ("Client> QUERY packet sent to server.\n");

    if (!XdmcpClientReceivePacket (client)) return FALSE;

    switch (client->header.opcode)
    {
    case WILLING:
        break;
    case UNWILLING:
        printf ("Error> Server is unwilling to accept remote login.\n");
        return FALSE;
    default:
        printf ("Error> Unexpected response %i\n", client->header.opcode);
        return FALSE;
    }

    ret = FALSE;
    if (XdmcpReadARRAY8 (&client->buffer, &authenname) &&
        XdmcpReadARRAY8 (&client->buffer, &hostname) &&
        XdmcpReadARRAY8 (&client->buffer, &status))
    {
        printf ("Server> WILLING (authen:");
        PrintARRAY8 (&authenname, TRUE);
        printf (" host:");
        PrintARRAY8 (&hostname, TRUE);
        printf (" status:");
        PrintARRAY8 (&status, TRUE);
        printf (")\n");
        if (client->header.length == 6 + authenname.length + hostname.length + status.length)
        {
            ret = XdmcpClientSetAuth (client, &authenname);
            if (!ret)
            {
                printf ("Error> Unsupported authentication name\n");
            }
        }
        else
        {
            printf ("Error> Header length unmatched\n");
        }
    }
    else
    {
        printf ("Error> Corrupted willing packet\n");
    }
    XdmcpDisposeARRAY8 (&authenname);
    XdmcpDisposeARRAY8 (&hostname);
    XdmcpDisposeARRAY8 (&status);

    return ret;
}

int
XdmcpClientAddXauth (XdmcpClient *client)
{
    char *cmd;
    int len;
    char *authorname, *authordata;
    int ret;

    authorname = ConvertARRAY8toString (&client->AuthorName, TRUE);
    authordata = ConvertARRAY8toString (&client->AuthorData, FALSE);

    len = 20 + strlen (authorname) + strlen (authordata);
    cmd = (char*) malloc (len + 1);
    snprintf (cmd, len, "xauth add :%i %s %s", client->DisplayNumber, authorname, authordata);
    ret = system (cmd);

    printf ("Client> '%s' returns %i\n", cmd, ret);

    free (authorname);
    free (authordata);
    free (cmd);

    return (ret == 0);
}

int
XdmcpClientRequest (XdmcpClient *client)
{
    int ret;
    ARRAY8  AcceptAuthenName = { 0 }, AcceptAuthenData = { 0 };

    client->header.version = XDM_PROTOCOL_VERSION;
    client->header.opcode = (CARD16) REQUEST; 

    client->header.length = 2; /* display number */
    /* TODO: Connection Types */
    client->header.length += 1 + 2 * 0; /* connection types */
    client->header.length += 1; /* connection addresses */
    /* for each connection type, += 2 + data.length */
    client->header.length += 2 + client->AuthenName.length;
    client->header.length += 2 + client->AuthenData.length;
    client->header.length += 1; /* authorization names */
    client->header.length += ARRAY8ofARRAY8Len (&client->AuthorNames);
    client->header.length += 2 + client->DisplayID.length;

    XdmcpWriteHeader (&client->buffer, &client->header);
    XdmcpWriteCARD16 (&client->buffer, (CARD16) client->DisplayNumber);
    XdmcpWriteCARD8 (&client->buffer, (CARD8) 0);
    /* connection types here */
    XdmcpWriteCARD8 (&client->buffer, (CARD8) 0);
    /* connection addresses here */
    XdmcpWriteARRAY8 (&client->buffer, &client->AuthenName);
    XdmcpWriteARRAY8 (&client->buffer, &client->AuthenData);
    XdmcpWriteARRAYofARRAY8 (&client->buffer, &client->AuthorNames);
    XdmcpWriteARRAY8 (&client->buffer, &client->DisplayID);

    XdmcpFlush (client->Sock, &client->buffer, (XdmcpNetaddr) &client->Addr, client->AddrLen);

    printf ("Client> REQUEST packet sent.\n");

    if (!XdmcpClientReceivePacket (client)) return FALSE;

    switch (client->header.opcode)
    {
    case ACCEPT:
        break;
    case DECLINE:
        printf ("Error> Server declined remote login request.\n");
        return FALSE;
    default:
        printf ("Error> Unexpected response %i\n", client->header.opcode);
        return FALSE;
    }

    ret = FALSE;

    if (XdmcpReadCARD32 (&client->buffer, &client->SessionID) &&
        XdmcpReadARRAY8 (&client->buffer, &AcceptAuthenName) &&
        XdmcpReadARRAY8 (&client->buffer, &AcceptAuthenData) &&
        XdmcpReadARRAY8 (&client->buffer, &client->AuthorName) &&
        XdmcpReadARRAY8 (&client->buffer, &client->AuthorData))
    {
        printf ("Server> ACCEPT (sessionid:%u authen:", (unsigned int) client->SessionID);
        PrintARRAY8 (&AcceptAuthenName, TRUE);
        printf (" authendata:");
        PrintARRAY8 (&AcceptAuthenData, FALSE);
        printf (" author:");
        PrintARRAY8 (&client->AuthorName, TRUE);
        printf (" authordata:");
        PrintARRAY8 (&client->AuthorData, FALSE);
        printf (")\n");
        ret = TRUE;
    }
    else
    {
        printf ("Error> Corrupted accept packet\n");
    }

    return ret;
}

int
XdmcpClientManage (XdmcpClient *client, unsigned int sessionid)
{
    client->header.version = XDM_PROTOCOL_VERSION;
    client->header.opcode = (CARD16) MANAGE;
    client->header.length = 8 + client->DisplayClass.length;

    if (sessionid > 0)
    {
        client->SessionID = (CARD32) sessionid;
    }

    XdmcpWriteHeader (&client->buffer, &client->header);
    XdmcpWriteCARD32 (&client->buffer, client->SessionID);
    XdmcpWriteCARD16 (&client->buffer, (CARD16) client->DisplayNumber);
    XdmcpWriteARRAY8 (&client->buffer, &client->DisplayClass);

    XdmcpFlush (client->Sock, &client->buffer, (XdmcpNetaddr) &client->Addr, client->AddrLen);

    printf ("Client> MANAGE packet sent for session %u.\n", (unsigned int) client->SessionID);

    return TRUE;
}

