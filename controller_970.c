#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9410
#define AUTH_TOKEN "OPS-0970"

int main(void)
{
    int sockfd;
    struct sockaddr_in server_addr;

    char buffer[1024];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET,
                  SERVER_IP,
                  &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sockfd);
        return 1;
    }

    if (connect(sockfd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("Connected to RemoteOps Agent.\n");

    char auth_command[128];

    snprintf(auth_command,
             sizeof(auth_command),
             "AUTH %s\n",
             AUTH_TOKEN);

    send(sockfd,
         auth_command,
         strlen(auth_command),
         0);

    printf("Sent: AUTH %s\n", AUTH_TOKEN);

    memset(buffer, 0, sizeof(buffer));

    int bytes_received =
        recv(sockfd,
             buffer,
             sizeof(buffer) - 1,
             0);

    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';

        printf("Agent response: %s",
               buffer);
    }

    close(sockfd);

    return 0;
}
