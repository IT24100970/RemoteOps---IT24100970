#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9410
#define AUTH_TOKEN "OPS-0970"
#define BUFFER_SIZE 1024


int recv_line(int sockfd,
              char *buffer,
              int max_size)
{
    int index = 0;
    char ch;
    int n;

    while (index < max_size - 1)
    {
        n = recv(sockfd,
                 &ch,
                 1,
                 0);

        if (n == 0)
        {
            return 0;
        }

        if (n < 0)
        {
            return -1;
        }

        if (ch == '\n')
        {
            break;
        }

        if (ch != '\r')
        {
            buffer[index++] = ch;
        }
    }

    buffer[index] = '\0';

    return index;
}


int send_all(int sockfd,
             const char *buffer,
             int length)
{
    int total_sent = 0;

    while (total_sent < length)
    {
        int sent =
            send(sockfd,
                 buffer + total_sent,
                 length - total_sent,
                 0);

        if (sent <= 0)
        {
            return -1;
        }

        total_sent += sent;
    }

    return total_sent;
}


int main(void)
{
    int sockfd;

    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];


    sockfd =
        socket(AF_INET,
               SOCK_STREAM,
               0);

    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }


    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family =
        AF_INET;

    server_addr.sin_port =
        htons(PORT);


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


    /* Authentication */

    snprintf(command,
             sizeof(command),
             "AUTH %s\n",
             AUTH_TOKEN);

    send_all(sockfd,
             command,
             strlen(command));


    if (recv_line(sockfd,
                  buffer,
                  sizeof(buffer)) <= 0)
    {
        printf("No authentication response.\n");
        close(sockfd);
        return 1;
    }


    printf("Agent: %s\n",
           buffer);


    if (strncmp(buffer,
                "OK AUTHENTICATED",
                16) != 0)
    {
        printf("Authentication failed.\n");

        close(sockfd);

        return 1;
    }


    /* Command loop */

    while (1)
    {
        printf("\nremoteops> ");

        if (fgets(command,
                  sizeof(command),
                  stdin) == NULL)
        {
            break;
        }


        /* Make sure command ends with newline */
        if (strchr(command,
                   '\n') == NULL)
        {
            strcat(command,
                   "\n");
        }


        send_all(sockfd,
                 command,
                 strlen(command));


        if (recv_line(sockfd,
                      buffer,
                      sizeof(buffer)) <= 0)
        {
            printf("Agent disconnected.\n");
            break;
        }


        printf("Agent: %s\n",
               buffer);


        if (strncmp(buffer,
                    "OK BYE",
                    6) == 0)
        {
            break;
        }
    }


    close(sockfd);

    printf("Controller closed.\n");

    return 0;
}

