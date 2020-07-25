/**
* Chatroom Lab
* CS 241 - Fall 2018
*/

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include "utils.h"

#define MAX_CLIENTS     8
#define LISTEN_BACKLOG  64

void *process_client(void *p);

static volatile int serverSocket;
static volatile int endSession;

static volatile int clientsCount;
static volatile int clients[MAX_CLIENTS];

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Signal handler for SIGINT.
 * Used to set flag to end server.
 */
void close_server() {
    endSession = 1;

    // add any additional flags here you want.

}

/**
 * Cleanup function called in main after `run_server` exits.
 * Server ending clean up (such as shutting down clients) should be handled
 * here.
 */
void cleanup() {
    // if (shutdown(serverSocket, SHUT_RDWR) != 0) {
    //     perror("shutdown server");
    // }
    close(serverSocket);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != -1) {
            if (shutdown(clients[i], SHUT_RDWR) != 0) {
                perror("shutdown client");
            }
            close(clients[i]);
        }
    }
}

/**
 * Sets up a server connection.
 * Does not accept more than MAX_CLIENTS connections.  If more than MAX_CLIENTS
 * clients attempts to connects, simply shuts down
 * the new client and continues accepting.
 * Per client, a thread should be created and 'process_client' should handle
 * that client.
 * Makes use of 'endSession', 'clientsCount', 'client', and 'mutex'.
 *
 * port - port server will run on.
 *
 * If any networking call fails, the appropriate error is printed and the
 * function calls exit(1):
 *    - fprtinf to stderr for getaddrinfo
 *    - perror() for any other call
 */
void run_server(char *port) {

    printf(">>> Creating socket... \n");
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket == -1) {
        perror("socket");
        exit(1);
    }
    printf(">>> Done\n");

    printf(">>> Setting up socket... \n");
    int optval = 1;
    int resultFromSSO = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEPORT, &optval, 
                                   sizeof(optval));
    if(resultFromSSO == -1) {
        perror("setsockopt");
        exit(1);
    }
    printf(">>> Done\n");


    printf(">>> Getting addr info... \n");
    struct addrinfo hints; 
    struct addrinfo* result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int resultFromGAI = getaddrinfo(NULL, port, &hints, &result);
    if (resultFromGAI != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(resultFromGAI));
        exit(1);
    }
    printf(">>> Done\n");

    /* bind */
    printf(">>> Binding...\n");
    if (bind(serverSocket, result->ai_addr, result->ai_addrlen) != 0) {
        perror("bind");
        exit(1);
    }
    printf(">>> Done\n");

    /* listen */
    printf(">>> Listening... \n");
    if (listen(serverSocket, LISTEN_BACKLOG) != 0) {
        perror("listen");
        exit(1);
    }
    printf(">>> Done\n");

    struct sockaddr_in* result_addr = (struct sockaddr_in*) result->ai_addr;
    printf(">>> Listening on file descriptor %d, port %d\n", 
            serverSocket, ntohs(result_addr->sin_port));
    
    // Clean up and set up client array
    freeaddrinfo(result);  result = NULL;  /* Don't need it */
    for(int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = -1;
    }

    // Huge while loop to accept connections
    while(!endSession) {

        struct sockaddr clientAddr;
        socklen_t clientAddrLen = sizeof(struct sockaddr);
        memset(&clientAddr, 0, sizeof(struct sockaddr));

        printf(">>> Waiting for connection...\n");
        int client_fd = accept(serverSocket, (struct sockaddr*) &clientAddr, &clientAddrLen);
        if(client_fd == -1) {
            // If something went wrong
            int errsv = errno;
            if(errsv == EINTR) {
                // accept interrupted by a SIGINT
                if(endSession) {
                    printf(">>> Ending session...\n");
                    // if (serverSocket) {
                    //     printf(">>> serverSocket still viable.\n");
                    // }
                }
                return;  // Return and clean up.
            }
            perror("accept");
            exit(1);
        }

        // Check if we've reached the MAX_CLIENTS limit
        if(clientsCount >= MAX_CLIENTS) {
            // If we've reached the limit, shutdown new client
            printf(">>> Connection denied: MAX_CLIENTS reached\n");
            printf(">>> Shutting down new client...\n");
            if (shutdown(client_fd, SHUT_RDWR) != 0) {
                perror("shutdown");
            }
            close(client_fd);
            printf(">>> Done\n");
        }
        else {
            // Else confirm connection, log info, and launch thread.
            printf(">>> Connection made: client_fd = %d\n", client_fd);
            intptr_t clientID = -1;
            pthread_mutex_lock(&mutex);     // Lock! 
            for(int i = 0; i < MAX_CLIENTS; i++) {
                if(clients[i] == -1) {
                    // This client slot is available
                    clients[i] = client_fd;

                    // Getting client addr info
                    char host[256], client_port[256];
                    printf(">>> Getting client info...\n");
                    int resultFromGetName = getnameinfo((struct sockaddr*) &clientAddr, clientAddrLen, host, sizeof(host), client_port, sizeof(client_port), NI_NUMERICHOST | NI_NUMERICSERV);
                    if (resultFromGetName != 0) {
                        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(resultFromGetName));
                        exit(1);
                    }
                    printf(">>> Client %d:\t IP: %s\t Port: %s\n", i, host, client_port);

                    clientID = i;
                    break;
                }
            }
            assert(clientID != -1);  // Sanity check. Remove me! 
            clientsCount++;
            printf(">>> Client ID:  \t %ld\n", clientID);
            printf(">>> Client count:\t %d\n", clientsCount);
            pthread_mutex_unlock(&mutex);   // Unlock!

            // Launch a thread to handle read/write
            printf(">>> Launching new thread... \n");
            pthread_t newThread;
            int resultFromCreate = pthread_create(&newThread, 0, process_client, (void*)clientID);
            if(resultFromCreate != 0) {
                perror("pthread_create");
                exit(1);
            }
            printf(">>> Done\n");

        } /* End of else */

    } /* End of while */

}


/**
 * Broadcasts the message to all connected clients.
 *
 * message  - the message to send to all clients.
 * size     - length in bytes of message to send.
 */
void write_to_clients(const char *message, size_t size) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != -1) {
            ssize_t retval = write_message_size(size, clients[i]);
            if (retval > 0) {
                retval = write_all_to_socket(clients[i], message, size);
            }
            if (retval == -1) {
                perror("write(): ");
            }
        }
    }
    pthread_mutex_unlock(&mutex);
}

/**
 * Handles the reading to and writing from clients.
 *
 * p  - (void*)intptr_t index where clients[(intptr_t)p] is the file descriptor
 * for this client.
 *
 * Return value not used.
 */
void *process_client(void *p) {
    pthread_detach(pthread_self());
    intptr_t clientId = (intptr_t)p;
    ssize_t retval = 1;
    char *buffer = NULL;

    while (retval > 0 && endSession == 0) {
        retval = get_message_size(clients[clientId]);
        if (retval > 0) {
            buffer = calloc(1, retval);
            retval = read_all_from_socket(clients[clientId], buffer, retval);
        }
        if (retval > 0)
            write_to_clients(buffer, retval);

        free(buffer);
        buffer = NULL;
    }

    printf("User %d left\n", (int)clientId);
    close(clients[clientId]);

    pthread_mutex_lock(&mutex);
    clients[clientId] = -1;
    clientsCount--;
    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "%s <port>\n", argv[0]);
        return -1;
    }

    struct sigaction act;
    memset(&act, '\0', sizeof(act));
    act.sa_handler = close_server;
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("sigaction");
        return 1;
    }

    // signal(SIGINT, close_server);  /* No need to uncomment */
    run_server(argv[1]);
    cleanup();
    pthread_exit(NULL);
}
