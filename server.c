// Netstick - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.
#include "server.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <linux/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/epoll.h>

//---------------------------------------------------------------------------
server_context_t* server_create(uint16_t port_, int maxClients_, client_handlers_t* clientHandlers_)
{
    int rc = socket(AF_INET, SOCK_STREAM, 0);
    if (rc < 0) {
        printf("error creating socket: %d (%s)\n", errno, strerror(errno));
        return NULL;
    }

    int fd     = rc;
    int enable = 1;
    rc         = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &enable, sizeof(enable));
    if (rc != 0) {
        printf("error setting socket option: %d (%s)\n", errno, strerror(errno));
        close(fd);
        return NULL;
    }

    struct sockaddr_in addr = {};
    addr.sin_family         = AF_INET;
    addr.sin_addr.s_addr    = INADDR_ANY;
    addr.sin_port           = htons(port_);

    rc = bind(fd, (const struct sockaddr*)&addr, sizeof(addr));
    if (rc < 0) {
        printf("error binding socket: %d (%s)\n", errno, strerror(errno));
        close(fd);
        return NULL;
    }

    rc = listen(fd, 4);
    if (rc < 0) {
        printf("error listening on socket: %d (%s)\n", errno, strerror(errno));
        close(fd);
        return NULL;
    }

    // create a context object and return it
    server_context_t* context = (server_context_t*)(calloc(1, sizeof(server_context_t)));
    context->port             = port_;
    context->serverFd         = fd;
    context->maxClients       = maxClients_;
    context->handlers         = *clientHandlers_;
    context->clientContext    = (client_context_t**)(calloc(1, sizeof(client_context_t*) * maxClients_));

    for (int i = 0; i < maxClients_; i++) {
        context->clientContext[i]              = (client_context_t*)(calloc(1, sizeof(client_context_t)));
        context->clientContext[i]->inUse       = false;
        context->clientContext[i]->clientFd    = -1;
        context->clientContext[i]->contextData = NULL;
    }
    return context;
}

//---------------------------------------------------------------------------
static void server_register_client_fd(int ePollFd_, int clientFd_)
{
    struct epoll_event ev = {};
    ev.events             = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLET;
    ev.data.fd            = clientFd_;
    if (epoll_ctl(ePollFd_, EPOLL_CTL_ADD, clientFd_, &ev) < 0) {
        printf("error registering client fd=%d: %d (%s)\n", clientFd_, errno, strerror(errno));
        exit(-1);
    }
}

//---------------------------------------------------------------------------
static void server_deregister_client_fd(int ePollFd_, int clientFd_)
{
    if (epoll_ctl(ePollFd_, EPOLL_CTL_DEL, clientFd_, NULL) < 0) {
        printf("error deregistering client fd=%d: %d (%s)\n", clientFd_, errno, strerror(errno));
        exit(-1);
    }
}

//---------------------------------------------------------------------------
static void server_on_client_connect(server_context_t* context_, int ePollFd_, int clientFd_)
{
    bool noRoom = true;
    for (int i = 0; i < context_->maxClients; i++) {
        if (!context_->clientContext[i]->inUse) {
            noRoom                                  = false;
            context_->clientContext[i]->inUse       = true;
            context_->clientContext[i]->clientFd    = clientFd_;
            context_->clientContext[i]->contextData = context_->handlers.onConnect(clientFd_);

            // Make non-blocking.
            int flags = fcntl(clientFd_, F_GETFL);
            flags |= O_NONBLOCK;
            fcntl(clientFd_, F_SETFL, flags);

            // Enable TCP keepalives on the socket
            int rc;
            int enable = 1;
            rc         = setsockopt(clientFd_, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));
            if (rc != 0) {
                printf("Error enabling socket keepalives on client\n");
            }

            // Set the timing parameters for dead "Idle" socket checks.

            // Check for dead idle connections on 10s of inactivity
            int idleTime = 10;
            rc           = setsockopt(clientFd_, SOL_TCP, TCP_KEEPIDLE, &idleTime, sizeof(idleTime));
            if (rc != 0) {
                printf("Error setting initial idle-time value\n");
            }

            // Set a maximum number of idle-socket heartbeat attemtps before assuming an idle socket it dead
            int keepCount = 5;
            rc            = setsockopt(clientFd_, SOL_TCP, TCP_KEEPCNT, &keepCount, sizeof(keepCount));
            if (rc != 0) {
                printf("Error setting idle retry count\n");
            }

            // On performing the socket-idle check, send heartbeat attempts on a specified interval
            int keepInterval = 5;
            rc               = setsockopt(clientFd_, SOL_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(keepInterval));
            if (rc != 0) {
                printf("Error setting idle retry interval\n");
            }

            break;
        }
    }

    if (noRoom) {
        close(clientFd_);
        printf("can't accept socket - too many clients connected\n");
        return;
    }

    server_register_client_fd(ePollFd_, clientFd_);
}

//---------------------------------------------------------------------------
static void server_on_client_disconnect(server_context_t* context_, int ePollFd_, int index_)
{
    context_->handlers.onDisconnect(context_->clientContext[index_]->contextData);
    server_deregister_client_fd(ePollFd_, context_->clientContext[index_]->clientFd);
    close(context_->clientContext[index_]->clientFd);
    context_->clientContext[index_]->clientFd = -1;
    context_->clientContext[index_]->inUse    = false;
}

//---------------------------------------------------------------------------
void server_run(server_context_t* context_)
{
    int ePollFd = epoll_create1(0);

    server_register_client_fd(ePollFd, context_->serverFd);

    while (1) {
        struct epoll_event ev;
        int                nfds = epoll_wait(ePollFd, &ev, 1, -1);
        if (nfds < 0) {
            printf("error on epoll_wait() = %d (%s)\n", errno, strerror(errno));
            return;
        }

        // Handle incoming connections on the registered socket.
        if (ev.data.fd == context_->serverFd) {
            struct sockaddr_in addr;
            socklen_t          socklen  = sizeof(addr);
            int                clientFd = accept(context_->serverFd, (struct sockaddr*)(&addr), &socklen);
            if (clientFd < 0) {
                printf("error accepting socket %d (%s)\n", errno, strerror(errno));
                return;
            }
            server_on_client_connect(context_, ePollFd, clientFd);
        } else {
            // Handle all other events...
            for (int i = 0; i < context_->maxClients; i++) {
                if (context_->clientContext[i]->clientFd == ev.data.fd) {
                    bool error = false;
                    if ((ev.events & EPOLLHUP) || (ev.events & EPOLLERR) || (ev.events & EPOLLRDHUP)) {
                        error = true;
                    } else if (ev.events & EPOLLIN) {
                        if (!context_->handlers.onReadData(ev.data.fd, context_->clientContext[i]->contextData)) {
                            error = true;
                        }
                    }

                    if (error) {
                        server_on_client_disconnect(context_, ePollFd, i);
                    }
                    break;
                }
            }
        }
    }
}
