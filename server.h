// Netstick - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

//---------------------------------------------------------------------------
// Function pointers used to implement the event-handlers for socket events
//---------------------------------------------------------------------------
typedef void* (*client_connect_handler_t)(int clientFd_);
typedef void (*client_disconnect_handler_t)(void* clientContext_);
typedef bool (*client_read_data_t)(int clientFd_, void* clientContext_);

//---------------------------------------------------------------------------
// Struct containing the handler functions for client events
typedef struct {
    client_connect_handler_t    onConnect;      //!< Action called when socket is connected
    client_disconnect_handler_t onDisconnect;   //!< Action called when the socket is disconnected
    client_read_data_t          onReadData;     //!< Action called when there is data to read on the socket
} client_handlers_t;

//---------------------------------------------------------------------------
// Struct describing the data
typedef struct {
    bool  inUse;        //!< Whether or not the context object is idle or active
    int   clientFd;     //!< FD corresponding to the socket
    void* contextData;  //!< Connection-specific pointer to application-specific data
} client_context_t;

//---------------------------------------------------------------------------
// Struct describing the server's complete context
typedef struct {
    uint16_t           port;            //!< port that the server is registered for
    int                serverFd;        //!< file descriptor of the active server
    int                maxClients;      //!< maximum number of concurrent connections allowed in the server
    client_handlers_t  handlers;        //!< event handler actions for the clients
    client_context_t** clientContext;   //!< array of context pointers, used to hold instance-specific application data
} server_context_t;

//---------------------------------------------------------------------------
/**
 * @brief server_create create a server that listens for incoming connections
 * on a given port.
 * @param port_ Port on which to listen for incoming connections
 * @param maxClients_ Maximum number of concurrent client connections
 * @param clientHandlers_ Pointer to an array of function pointers describing
 * @return pointer to a newly-constructed active server_context_t on success, NULL on error
 */
server_context_t* server_create(uint16_t port_, int maxClients_, client_handlers_t* clientHandlers_);

//---------------------------------------------------------------------------
/**
 * @brief server_run Run the server's activities.  This effectively takes over
 * the caller's thread and will not return unless some catastrophic error has
 * occurred that results in our server dying.
 * @param context_ pointer to the server_context_t object that describes the
 * behavior of a server.
 */
void server_run(server_context_t* context_);

#if defined(__cplusplus)
}
#endif
