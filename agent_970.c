#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9410
#define BACKLOG 5
#define AUTH_TOKEN "OPS-0970"
#define SID_TAG "SID:0790"

int main(void)
{
    int server_fd;
    int client_fd;
    int opt = 1;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len = sizeof(client_addr);

    char buffer[1024];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    if (setsockopt(server_fd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &opt,
                   sizeof(opt)) < 0)
    {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("RemoteOps Agent listening on port %d...\n", PORT);

    client_fd = accept(server_fd,
                       (struct sockaddr *)&client_addr,
                       &client_len);

    if (client_fd < 0)
    {
        perror("accept");
        close(server_fd);
        return 1;
    }

    printf("Controller connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));

    memset(buffer, 0, sizeof(buffer));

    int bytes_received = recv(client_fd,
                              buffer,
                              sizeof(buffer) - 1,
                              0);

    if (bytes_received <= 0)
    {
        printf("Controller disconnected before authentication.\n");
        close(client_fd);
        close(server_fd);
        return 0;
    }

    buffer[bytes_received] = '\0';

    /* Remove newline if present */
    buffer[strcspn(buffer, "\r\n")] = '\0';

    printf("Received command: %s\n", buffer);

    if (strcmp(buffer, "AUTH " AUTH_TOKEN) == 0)
    {
        const char *response =
            "OK AUTHENTICATED " SID_TAG "\n";

        send(client_fd,
             response,
             strlen(response),
             0);

        printf("Authentication successful.\n");
    }
    else
    {
        const char *response =
            "ERR 001 AUTH_FAILED " SID_TAG "\n";

        send(client_fd,
             response,
             strlen(response),
             0);

        printf("Authentication failed.\n");
    }

    close(client_fd);
    close(server_fd);

    printf("Connection closed.\n");

    return 0;
}
