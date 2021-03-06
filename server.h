#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

//---------------------------------------------------------------------------
typedef void* (*client_connect_handler_t)(int clientFd_);
typedef void (*client_disconnect_handler_t)(void* clientContext_);
typedef bool (*client_read_data_t)(int clientFd_, void* clientContext_);

//---------------------------------------------------------------------------
typedef struct {
	client_connect_handler_t 	onConnect;
	client_disconnect_handler_t onDisconnect;
	client_read_data_t			onReadData;
} client_handlers_t;

//---------------------------------------------------------------------------
typedef struct {
	bool inUse;
	int clientFd;
	void* contextData;
} client_context_t;

//---------------------------------------------------------------------------
typedef struct {
	uint16_t 		port;
	int 			serverFd;
	int				maxClients;
	client_handlers_t handlers;
	client_context_t** clientContext;
} server_context_t;

//---------------------------------------------------------------------------
server_context_t* server_create(uint16_t port_, int maxClients_, client_handlers_t* clientHandlers_);

//---------------------------------------------------------------------------
void server_run(server_context_t* context_);

#if defined(__cplusplus)
}
#endif