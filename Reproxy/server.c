#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "util.h"

//#define DEBUG

#define SERV_PORT 23333
#define REMOTE_IP "210.44.144.3"
//#define REMOTE_IP "172.25.64.1"
//#define REMOTE_IP "47.94.42.154"
#define REMOTE_PORT 80

#define MAX_BUFFER 8192

int server_socket;
int client_socket;
int remote_socket;

char remote_host[128];
int remote_port;
char client_host[128];
int client_port;

struct http_request
{
    char *method;
    char *url;
    char *version;
    struct Map *headers;
    char *body;
};

int connect_remote();
int creat_server_socket(int port);
void forward_data(int source_socket, int destination_socket);
void handle_client(struct sockaddr_in client_addr);
void server_deal();
void sigchld_handler(int signal);

int creat_server_socket(int port)
{
    int server_socket, optval = 1;
    struct sockaddr_in server_addr;

    server_socket = Socket(AF_INET, SOCK_STREAM, 0);

#ifdef DEBUG
    printf("creat server socket\n");
#endif

    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    Bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));

    Listen(server_socket, 128);

    return server_socket;
}

int connect_remote()
{
    struct sockaddr_in remote_server_addr;
    int socket;

    socket = Socket(AF_INET, SOCK_STREAM, 0);

    bzero(&remote_server_addr, sizeof(remote_server_addr));
    remote_server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, REMOTE_IP, &remote_server_addr.sin_addr);
    remote_server_addr.sin_port = htons(REMOTE_PORT);

    Connect(socket, (struct sockaddr *)&remote_server_addr, sizeof(remote_server_addr));

    return socket;
}

void parse_client_request(char *buffer, int len, struct http_request *request)
{
    if (len == 0)
    {
        return;
    }

    char *data = (char *)malloc(len);
    memcpy(data, buffer, len);
    char *start = data;
    char *method = start;
    char *url = 0;
    char *version = 0;
    for (; *start && *start != '\n' && *start != '\r'; start++)
    {
        if (*start == ' ')
        {
            if (url == 0)
            {
                url = start + 1;
            }
            else
            {
                version = start + 1;
            }
            *start = '\0';
        }
    }
    *start = '\0';
    start++;
    request->method = method;
    request->url = url;
    request->version = version;

    start++;
    char *line = start;
    char *key;
    char *value;
    while (*line != '\r' && *line != '\0')
    {
        char *key;
        char *value;
        while (*(start++) != ':')
            ;
        *(start - 1) = '\0';
        key = line;
        value = start + 1;
        while (start++, *start != '\0' && *start != '\r')
            ;
        *start++ = '\0';
        start++;

        line = start;
        struct Item *item = newItem(key, value);
        mapPush(request->headers, item);
    }

    if (*line == '\r')
    {
        char *len_str = mapGet(request->headers, "Content-Length");
        if (len_str != NULL)
        {
            int len = atoi(len_str);
            line = line + 2;
            *(line + len) = '\0';
            request->body = line;
        }
    }
    //mapPrint(request->headers);
}

void forward_data(int source_socket, int destination_socket)
{
    char buffer[MAX_BUFFER];
    struct http_request request;
    int n;

    while ((n = recv(source_socket, buffer, MAX_BUFFER, 0)) > 0)
    {
#ifdef DEBUG
        if (source_socket == client_socket)
        {
            printf("--------------------------------------------------------------\n");
            //printf("client[%s:%d] send to remote[%s:%d], size=%d, client request:\n%s\n", client_host, client_port, REMOTE_IP, REMOTE_PORT, n, buffer);
            printf("client[%s:%d] send to remote[%s:%d], size=%d\n", client_host, client_port, REMOTE_IP, REMOTE_PORT, n);
            printf("--------------------------------------------------------------\n");
        }
        if (source_socket == remote_socket)
        {
            //printf("remote[%s:%d] send to client[%s:%d], size=%d\n", REMOTE_IP, REMOTE_PORT, client_host, client_port, n);
        }
#endif
        if (source_socket == client_socket)
        {
            if (request.headers == NULL)
            {
                struct Map headers;
                request.headers = &headers;
            }
            initMap(request.headers);
            parse_client_request(buffer, n, &request);
            printf("[%s:%d][%s][%s][%s]\n", client_host, client_port, request.method, request.url, request.version);
#ifdef DEBUG
            mapPrint(request.headers);
            if(request.body != NULL)
            {
                printf("%s\n", request.body);
            }
#endif
        }
        send(destination_socket, buffer, n, 0);
    }
    if (source_socket == client_socket)
    {
        releaseMap(request.headers);
    }
    shutdown(destination_socket, SHUT_RDWR);
    shutdown(source_socket, SHUT_RDWR);
}

void handle_client(struct sockaddr_in client_addr)
{

    remote_socket = connect_remote();

    if (fork() == 0)
    {
        forward_data(client_socket, remote_socket);
        Close(client_socket);
        Close(remote_socket);
        exit(0);
    }

    if (fork() == 0)
    {
        forward_data(remote_socket, client_socket);
        Close(remote_socket);
        Close(client_socket);
        exit(0);
    }
    Close(remote_socket);
    Close(client_socket);
}

void server_deal()
{
    printf("server start listen...\n");

    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    char str[INET_ADDRSTRLEN];
    while (1)
    {
        client_socket = Accept(server_socket, (struct sockaddr *)&client_addr, &addrlen);

        strcpy(client_host, inet_ntop(AF_INET, &client_addr.sin_addr, str, sizeof(str)));
        client_port = ntohs(client_addr.sin_port);
#ifdef DEBUG
        printf("received request from %s:%d and client socket is %d\n",
               inet_ntop(AF_INET, &client_addr.sin_addr, str, sizeof(str)),
               ntohs(client_addr.sin_port), client_socket);
#endif
        if (fork() == 0)
        {
            Close(server_socket);
            handle_client(client_addr);
            exit(0);
        }

        Close(client_socket);
    }
}

void sigchld_handler(int signal)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
#ifdef DEBUG
        printf("destroy process\n");
#endif
    }
}

int main(int argc, char *argv[])
{
    signal(SIGCHLD, sigchld_handler);

    server_socket = creat_server_socket(SERV_PORT);

    server_deal();

    return 0;
}