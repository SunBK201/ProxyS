#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>
#include "util.h"

#define SERV_PORT 23333
#define REMOTE_IP "210.44.144.3"
#define REMOTE_PORT 80

#define MAX_BUFFER 8192

int server_socket;
int client_socket;
int remote_socket;

char remote_host[128];
int remote_port;
char client_host[128];
int client_port;

char *client_buffer;

int connect_remote();
int creat_server_socket(int port);
int extract_host(const char *header);
void forward_data(int source_socket, int destination_socket);
void handle_client(int client_socket, struct sockaddr_in client_addr);
int read_from_client(int client_socket);
void server_deal();
void sigchld_handler(int signal);

int creat_server_socket(int port)
{
    int server_socket, optval;
    struct sockaddr_in server_addr;

    server_socket = Socket(AF_INET, SOCK_STREAM, 0);

    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    Bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));

    Listen(server_socket, 128);

    printf("creat_server_socket\n");

    return server_socket;
}

int extract_host(const char *header)
{
    char *p = strstr(header, "Host:");

    char *p2 = strchr(p + 5, ':');

    char *p1 = strchr(p, '\n');

    if (p2 && p2 < p1)
    {

        int p_len = (int)(p1 - p2 - 1);
        char s_port[p_len];
        strncpy(s_port, p2 + 1, p_len);
        s_port[p_len] = '\0';
        remote_port = atoi(s_port);

        int h_len = (int)(p2 - p - 5 - 1);
        strncpy(remote_host, p + 5 + 1, h_len);
        remote_host[h_len] = '\0';
    }
    else
    {
        int h_len = (int)(p1 - p - 5 - 1 - 1);
        strncpy(remote_host, p + 5 + 1, h_len);
        remote_host[h_len] = '\0';
        remote_port = 80;
    }
    return 0;
}

int read_from_client(int client_socket)
{
    memset(client_buffer, 0, MAX_BUFFER);
    char line_buffer[2048];
    char *base_ptr = client_buffer;

    while (1)
    {
        int total_read = Readline(client_socket, line_buffer, 2048);

        if (base_ptr + total_read - client_buffer <= MAX_BUFFER)
        {
            strncpy(base_ptr, line_buffer, total_read);
            base_ptr += total_read;
        }

        if (strcmp(line_buffer, "\r\n") == 0 || strcmp(line_buffer, "\n") == 0)
        {
            break;
        }
    }
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

void forward_data(int source_socket, int destination_socket)
{
    char buffer[MAX_BUFFER];
    int n;

    while ((n = recv(source_socket, buffer, MAX_BUFFER, 0)) > 0)
    {
        if(source_socket == remote_socket)
        {
            printf("remote[%s:%d] data to server and send to client[%s:%d]\n", remote_host, remote_port, client_host, client_port);
        }
        if(source_socket == client_socket)
        {
            printf("client[%s:%d] data to server and send to remote[%s:%d]\n", client_host, client_port, remote_host, remote_port);
        }
        send(destination_socket, buffer, n, 0);
    }

    shutdown(destination_socket, SHUT_RDWR);

    shutdown(source_socket, SHUT_RDWR);
}

void handle_client(int client_socket, struct sockaddr_in client_addr)
{
    read_from_client(client_socket);

    //extract_host(client_buffer);

    remote_socket = connect_remote();

    if (fork() == 0)
    {

        if (strlen(client_buffer) > 0)
        {
            /*
            char *p = strstr(client_buffer, "CONNECT");
            printf("%s\n", client_buffer);
            if (p)
            {
                str_rep(client_buffer, client_buffer, "CONNECT", "GET");
                printf("%s\n", client_buffer);
            }
            */

            printf("send header to %s from client %s:%d\n", REMOTE_IP, client_host, client_port);
            send(remote_socket, client_buffer, strlen(client_buffer), 0);
        }
        
        forward_data(client_socket, remote_socket);
        exit(0);
    }

    if (fork() == 0)
    {

        forward_data(remote_socket, client_socket);
        exit(0);
    }

    Close(remote_socket);
    Close(client_socket);
}

void server_deal()
{
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    char str[INET_ADDRSTRLEN];
    while (1)
    {
        client_socket = Accept(server_socket, (struct sockaddr *)&client_addr, &addrlen);

        strcpy(client_host, inet_ntop(AF_INET, &client_addr.sin_addr, str, sizeof(str)));
        client_port = ntohs(client_addr.sin_port);

        printf("received from %s at PORT %d\n",
                inet_ntop(AF_INET, &client_addr.sin_addr, str, sizeof(str)),
                ntohs(client_addr.sin_port));

        if (fork() == 0)
        {
            close(server_socket);
            handle_client(client_socket, client_addr);
            exit(0);
        }
        Close(client_socket);
    }
}

void sigchld_handler(int signal)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

int main(int argc, char *argv[])
{
    client_buffer = (char *)malloc(MAX_BUFFER);

    signal(SIGCHLD, sigchld_handler);

    server_socket = creat_server_socket(SERV_PORT);

    server_deal();

    return 0;
}