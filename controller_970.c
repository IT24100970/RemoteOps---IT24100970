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


/* Receive one newline-terminated response */
int recv_line(int sockfd, char *buffer, int max_size)
{
    int index = 0;
    char ch;
    int n;

    while (index < max_size - 1)
    {
        n = recv(sockfd, &ch, 1, 0);

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


/* Send all bytes in a buffer */
int send_all(int sockfd, const char *buffer, int length)
{
    int total_sent = 0;

    while (total_sent < length)
    {
        int sent = send(sockfd,
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


/* Send exactly filesize bytes from a file */
int send_file_bytes(int sockfd,
                    FILE *fp,
                    long filesize)
{
    char file_buffer[4096];

    long total_sent = 0;

    while (total_sent < filesize)
    {
        long remaining =
            filesize - total_sent;

        size_t to_read =
            remaining < (long)sizeof(file_buffer)
            ? (size_t)remaining
            : sizeof(file_buffer);

        size_t bytes_read =
            fread(file_buffer,
                  1,
                  to_read,
                  fp);

        if (bytes_read == 0)
        {
            return -1;
        }

        if (send_all(sockfd,
                     file_buffer,
                     (int)bytes_read) < 0)
        {
            return -1;
        }

        total_sent += bytes_read;
    }

    return 0;
}


int main(void)
{
    int sockfd;

    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];


    /* Create TCP socket */
    sockfd = socket(AF_INET,
                    SOCK_STREAM,
                    0);

    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }


    /* Configure Agent address */
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


    /* Connect to Agent */
    if (connect(sockfd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sockfd);
        return 1;
    }


    printf("Connected to RemoteOps Agent.\n");


    /* -------------------------
       Authentication
       ------------------------- */

    snprintf(command,
             sizeof(command),
             "AUTH %s\n",
             AUTH_TOKEN);


    if (send_all(sockfd,
                 command,
                 strlen(command)) < 0)
    {
        printf("Failed to send authentication command.\n");
        close(sockfd);
        return 1;
    }


    if (recv_line(sockfd,
                  buffer,
                  sizeof(buffer)) <= 0)
    {
        printf("No authentication response from Agent.\n");
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


    /* -------------------------
       Command Loop
       ------------------------- */

    while (1)
    {
        printf("\nremoteops> ");

        if (fgets(command,
                  sizeof(command),
                  stdin) == NULL)
        {
            break;
        }


        /*
         * Remove newline temporarily.
         * This makes command checking easier.
         */
        command[strcspn(command, "\r\n")] = '\0';


        /* -------------------------
           PUT FILE UPLOAD
           ------------------------- */

        if (strncmp(command,
                    "PUT ",
                    4) == 0)
        {
            char filename[256];


            if (sscanf(command,
                       "PUT %255s",
                       filename) != 1)
            {
                printf("Usage: PUT <filename>\n");
                continue;
            }


            /* Open local file */
            FILE *fp =
                fopen(filename, "rb");

            if (fp == NULL)
            {
                perror("fopen");
                continue;
            }


            /* Find file size */
            if (fseek(fp,
                      0,
                      SEEK_END) != 0)
            {
                perror("fseek");
                fclose(fp);
                continue;
            }


            long filesize =
                ftell(fp);

            if (filesize < 0)
            {
                perror("ftell");
                fclose(fp);
                continue;
            }


            rewind(fp);


            /* Create required protocol header */
            char put_header[512];

            snprintf(put_header,
                     sizeof(put_header),
                     "PUT %s %ld\n",
                     filename,
                     filesize);


            /* Send PUT header */
            if (send_all(sockfd,
                         put_header,
                         strlen(put_header)) < 0)
            {
                printf("Failed to send PUT request.\n");

                fclose(fp);
                break;
            }


            printf("Uploading %s (%ld bytes)...\n",
                   filename,
                   filesize);


            /* Send raw file bytes */
            if (send_file_bytes(sockfd,
                                fp,
                                filesize) < 0)
            {
                printf("File upload failed.\n");

                fclose(fp);
                break;
            }


            fclose(fp);


            /* Wait for Agent response */
            if (recv_line(sockfd,
                          buffer,
                          sizeof(buffer)) <= 0)
            {
                printf("No response from Agent.\n");
                break;
            }


            printf("Agent: %s\n",
                   buffer);


            continue;
        }


        /* -------------------------
           Normal text commands
           ------------------------- */

        char protocol_command[BUFFER_SIZE];

        snprintf(protocol_command,
                 sizeof(protocol_command),
                 "%s\n",
                 command);


        if (send_all(sockfd,
                     protocol_command,
                     strlen(protocol_command)) < 0)
        {
            printf("Failed to send command.\n");
            break;
        }


        if (recv_line(sockfd,
                      buffer,
                      sizeof(buffer)) <= 0)
        {
            printf("Agent disconnected.\n");
            break;
        }


        printf("Agent: %s\n",
               buffer);


        /* Quit after OK BYE */
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
