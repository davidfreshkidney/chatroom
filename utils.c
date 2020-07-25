/**
* Chatroom Lab
* CS 241 - Fall 2018
*/

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "utils.h"
static const size_t MESSAGE_SIZE_DIGITS = 4;

char *create_message(char *name, char *message) {
    int name_len = strlen(name);
    int msg_len = strlen(message);
    char *msg = calloc(1, msg_len + name_len + 4);
    sprintf(msg, "%s: %s", name, message);

    return msg;
}

ssize_t get_message_size(int socket) {
    int32_t size;
    ssize_t read_bytes =
        read_all_from_socket(socket, (char *)&size, MESSAGE_SIZE_DIGITS);
    if (read_bytes == 0 || read_bytes == -1)
        return read_bytes;

    return (ssize_t)ntohl(size);
}

// You may assume size won't be larger than a 4 byte integer
ssize_t write_message_size(size_t size, int socket) {
    int32_t mesgSize = htonl(size);
    ssize_t write_bytes = 
            write_all_to_socket(socket, (char*)&mesgSize, MESSAGE_SIZE_DIGITS);
    if (write_bytes == 0 || write_bytes == -1)
        return write_bytes;

    return write_bytes;
}

ssize_t read_all_from_socket(int socket, char *buffer, size_t count) {

    ssize_t bytesRead = 0;
    ssize_t bytesRemaining = count;
    ssize_t resultFromRead = 0;
    errno = 0;  /* Reset errno */
    int errsv = 0;

    while(bytesRemaining > 0) {
        // While there is still bytes remaining to be read
        resultFromRead = read(socket, 
                        buffer + bytesRead /* Start from where we left off */, 
                        bytesRemaining /* Read rest of the bytes */);
        if(resultFromRead == 0) {
            // Nothing read
            return 0;
        }
        else if(resultFromRead > 0) {
            // Read something, log bytes read
            bytesRead      += resultFromRead;
            bytesRemaining -= resultFromRead;
        }
        else if(resultFromRead == -1) {
            // Something went wrong, log errno
            errsv = errno;
            if(errsv == EINTR){
                // Try again
                continue;
            }
            else {
                return -1;
            }
        }
        else {
            // Default
            return -1;
        }
    }

    return bytesRead;
}

ssize_t write_all_to_socket(int socket, const char *buffer, size_t count) {
    ssize_t bytesWritten = 0;
    ssize_t bytesRemaining = count;
    ssize_t resultFromWrite = 0;
    errno = 0;  /* Reset errno */
    int errsv = 0;

    while(bytesRemaining > 0) {
        // While there is still bytes remaining to be read
        resultFromWrite = write(socket, 
                        buffer + bytesWritten /* Start from where we left off */, 
                        bytesRemaining /* Read rest of the bytes */);
        if(resultFromWrite == 0) {
            // Nothing written
            return 0;
        }
        else if(resultFromWrite > 0) {
            // Wrote something, log bytes written
            bytesWritten    += resultFromWrite;
            bytesRemaining  -= resultFromWrite;
        }
        else if(resultFromWrite == -1) {
            // Something went wrong, log errno
            errsv = errno;
            if(errsv == EINTR){
                // Try again
                continue;
            }
            else {
                return -1;
            }
        }
        else {
            // Default
            return -1;
        }
    }

    return bytesWritten;
}
