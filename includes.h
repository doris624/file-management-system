#ifndef INCLUDES_H
#define INCLUDES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdbool.h>
#include <arpa/inet.h>

#define PORT 9003
#define SERVER_ADDR "127.0.0.1"
#define BUFFER_SIZE 1024
#define RESPONSE_SIZE 4096
#define CONTENT_SIZE 37690

struct User
{
    char name[256];
    char group[50];
};

// client structure
typedef struct
{
    struct User user;
    char command[BUFFER_SIZE];
} ClientRequest;

// server response structure
typedef struct
{
    char status[256];
    char content[BUFFER_SIZE];
} Response;

#endif