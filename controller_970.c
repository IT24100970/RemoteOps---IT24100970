#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/time.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9410

#define AUTH_TOKEN "OPS-0970"

#define BUFFER_SIZE 1024

#define UDP_PORT 9500


typedef struct
{
    int sockfd;

    volatile int active;

} udp_listener_t;


/* =========================================================
   Receive line
   ========================================================= */
int recv_line(int sockfd,
              char *buffer,
              int max_size)
{
    int index = 0;

    char ch;


    while (index < max_size - 1)
    {
        int n =
            recv(
                sockfd,
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


/* =========================================================
   Send all
   ========================================================= */
int send_all(int sockfd,
             const char *buffer,
             int length)
{
    int total_sent = 0;


    while (total_sent < length)
    {
        int sent =
            send(
                sockfd,
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


/* =========================================================
   Send file
   ========================================================= */
int send_file_bytes(int sockfd,
                    FILE *fp,
                    long filesize)
{
    char file_buffer[4096];


    long total_sent = 0;


    while (total_sent < filesize)
    {
        long remaining =
            filesize
            -
            total_sent;


        size_t to_read;


        if (remaining <
            (long)sizeof(file_buffer))
        {
            to_read =
                (size_t)remaining;
        }
        else
        {
            to_read =
                sizeof(file_buffer);
        }


        size_t bytes_read =
            fread(
                file_buffer,
                1,
                to_read,
                fp);


        if (bytes_read == 0)
        {
            return -1;
        }


        if (send_all(
                sockfd,
                file_buffer,
                (int)bytes_read) < 0)
        {
            return -1;
        }


        total_sent +=
            bytes_read;
    }


    return 0;
}


/* =========================================================
   Receive file
   ========================================================= */
int receive_file_bytes(int sockfd,
                       FILE *fp,
                       long filesize)
{
    char file_buffer[4096];


    long total_received = 0;


    while (total_received < filesize)
    {
        long remaining =
            filesize
            -
            total_received;


        int to_receive;


        if (remaining <
            (long)sizeof(file_buffer))
        {
            to_receive =
                (int)remaining;
        }
        else
        {
            to_receive =
                sizeof(file_buffer);
        }


        int received =
            recv(
                sockfd,
                file_buffer,
                to_receive,
                0);


        if (received <= 0)
        {
            return -1;
        }


        size_t written =
            fwrite(
                file_buffer,
                1,
                received,
                fp);


        if (written !=
            (size_t)received)
        {
            return -1;
        }


        total_received +=
            received;
    }


    return 0;
}


/* =========================================================
   UDP listener
   ========================================================= */
void *udp_listener_thread(void *arg)
{
    udp_listener_t *listener =
        (udp_listener_t *)arg;


    char buffer[1024];


    struct sockaddr_in sender_addr;


    while (listener->active)
    {
        socklen_t sender_len =
            sizeof(sender_addr);


        int received =
            recvfrom(
                listener->sockfd,
                buffer,
                sizeof(buffer) - 1,
                0,
                (struct sockaddr *)&sender_addr,
                &sender_len);


        if (received > 0)
        {
            buffer[received] =
                '\0';


            printf(
                "\n[UDP Monitor] %s\n",
                buffer);


            printf(
                "remoteops> ");


            fflush(
                stdout);
        }
    }


    return NULL;
}


/* =========================================================
   Stop UDP listener
   ========================================================= */
void stop_udp_listener(udp_listener_t *listener,
                       pthread_t *thread,
                       int *thread_started)
{
    if (*thread_started)
    {
        listener->active =
            0;


        pthread_join(
            *thread,
            NULL);


        close(
            listener->sockfd);


        listener->sockfd =
            -1;


        *thread_started =
            0;
    }
}


/* =========================================================
   MAIN
   ========================================================= */
int main(void)
{
    int sockfd;


    struct sockaddr_in server_addr;


    char buffer[BUFFER_SIZE];

    char command[BUFFER_SIZE];


    int udp_sock =
        -1;


    udp_listener_t udp_listener;


    memset(
        &udp_listener,
        0,
        sizeof(udp_listener));


    udp_listener.sockfd =
        -1;


    pthread_t udp_thread;


    int udp_thread_started =
        0;


    /* -----------------------------------------------------
       TCP socket
       ----------------------------------------------------- */
    sockfd =
        socket(
            AF_INET,
            SOCK_STREAM,
            0);


    if (sockfd < 0)
    {
        perror(
            "socket");


        return 1;
    }


    memset(
        &server_addr,
        0,
        sizeof(server_addr));


    server_addr.sin_family =
        AF_INET;


    server_addr.sin_port =
        htons(PORT);


    if (inet_pton(
            AF_INET,
            SERVER_IP,
            &server_addr.sin_addr) <= 0)
    {
        perror(
            "inet_pton");


        close(
            sockfd);


        return 1;
    }


    /* -----------------------------------------------------
       Connect
       ----------------------------------------------------- */
    if (connect(
            sockfd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)) < 0)
    {
        perror(
            "connect");


        close(
            sockfd);


        return 1;
    }


    printf(
        "Connected to RemoteOps Agent.\n");


    /* =====================================================
       AUTH
       ===================================================== */
    snprintf(
        command,
        sizeof(command),
        "AUTH %s\n",
        AUTH_TOKEN);


    if (send_all(
            sockfd,
            command,
            strlen(command)) < 0)
    {
        printf(
            "Failed to send authentication command.\n");


        close(
            sockfd);


        return 1;
    }


    if (recv_line(
            sockfd,
            buffer,
            sizeof(buffer)) <= 0)
    {
        printf(
            "No authentication response.\n");


        close(
            sockfd);


        return 1;
    }


    printf(
        "Agent: %s\n",
        buffer);


    if (strncmp(
            buffer,
            "OK AUTHENTICATED",
            16) != 0)
    {
        printf(
            "Authentication failed.\n");


        close(
            sockfd);


        return 1;
    }


    /* =====================================================
       COMMAND LOOP
       ===================================================== */
    while (1)
    {
        printf(
            "\nremoteops> ");


        if (fgets(
                command,
                sizeof(command),
                stdin) == NULL)
        {
            break;
        }


        command[
            strcspn(
                command,
                "\r\n")
        ] = '\0';


        /* -------------------------------------------------
           PUT
           ------------------------------------------------- */
        if (strncmp(
                command,
                "PUT ",
                4) == 0)
        {
            char filename[256];


            if (sscanf(
                    command,
                    "PUT %255s",
                    filename) != 1)
            {
                printf(
                    "Usage: PUT <filename>\n");


                continue;
            }


            FILE *fp =
                fopen(
                    filename,
                    "rb");


            if (fp == NULL)
            {
                perror(
                    "fopen");


                continue;
            }


            fseek(
                fp,
                0,
                SEEK_END);


            long filesize =
                ftell(
                    fp);


            rewind(
                fp);


            char put_header[512];


            snprintf(
                put_header,
                sizeof(put_header),
                "PUT %s %ld\n",
                filename,
                filesize);


            if (send_all(
                    sockfd,
                    put_header,
                    strlen(put_header)) < 0)
            {
                fclose(
                    fp);


                break;
            }


            printf(
                "Uploading %s (%ld bytes)...\n",
                filename,
                filesize);


            if (send_file_bytes(
                    sockfd,
                    fp,
                    filesize) < 0)
            {
                printf(
                    "File upload failed.\n");


                fclose(
                    fp);


                break;
            }


            fclose(
                fp);


            if (recv_line(
                    sockfd,
                    buffer,
                    sizeof(buffer)) <= 0)
            {
                break;
            }


            printf(
                "Agent: %s\n",
                buffer);


            continue;
        }


        /* -------------------------------------------------
           GET
           ------------------------------------------------- */
        if (strncmp(
                command,
                "GET ",
                4) == 0)
        {
            char filename[256];


            if (sscanf(
                    command,
                    "GET %255s",
                    filename) != 1)
            {
                printf(
                    "Usage: GET <filename>\n");


                continue;
            }


            char request[512];


            snprintf(
                request,
                sizeof(request),
                "GET %s\n",
                filename);


            if (send_all(
                    sockfd,
                    request,
                    strlen(request)) < 0)
            {
                break;
            }


            if (recv_line(
                    sockfd,
                    buffer,
                    sizeof(buffer)) <= 0)
            {
                break;
            }


            if (strncmp(
                    buffer,
                    "ERR ",
                    4) == 0)
            {
                printf(
                    "Agent: %s\n",
                    buffer);


                continue;
            }


            char received_filename[256];

            long filesize;

            char sid[64];


            if (sscanf(
                    buffer,
                    "OK FILE_SEND %255s %ld %63s",
                    received_filename,
                    &filesize,
                    sid) != 3)
            {
                printf(
                    "Invalid response.\n");


                break;
            }


            char output_filename[512];


            snprintf(
                output_filename,
                sizeof(output_filename),
                "downloaded_%s",
                received_filename);


            FILE *fp =
                fopen(
                    output_filename,
                    "wb");


            if (fp == NULL)
            {
                perror(
                    "fopen");


                break;
            }


            printf(
                "Downloading %s (%ld bytes)...\n",
                received_filename,
                filesize);


            if (receive_file_bytes(
                    sockfd,
                    fp,
                    filesize) < 0)
            {
                fclose(
                    fp);


                printf(
                    "Download failed.\n");


                break;
            }


            fclose(
                fp);


            printf(
                "File saved as: %s\n",
                output_filename);


            continue;
        }


        /* -------------------------------------------------
           MONITOR START
           ------------------------------------------------- */
        if (strcmp(
                command,
                "MONITOR START") == 0)
        {
            if (udp_thread_started)
            {
                printf(
                    "Monitoring already active.\n");


                continue;
            }


            udp_sock =
                socket(
                    AF_INET,
                    SOCK_DGRAM,
                    0);


            if (udp_sock < 0)
            {
                perror(
                    "UDP socket");


                continue;
            }


            struct timeval timeout;


            timeout.tv_sec =
                1;


            timeout.tv_usec =
                0;


            setsockopt(
                udp_sock,
                SOL_SOCKET,
                SO_RCVTIMEO,
                &timeout,
                sizeof(timeout));


            struct sockaddr_in udp_addr;


            memset(
                &udp_addr,
                0,
                sizeof(udp_addr));


            udp_addr.sin_family =
                AF_INET;


            udp_addr.sin_addr.s_addr =
                INADDR_ANY;


            udp_addr.sin_port =
                htons(
                    UDP_PORT);


            if (bind(
                    udp_sock,
                    (struct sockaddr *)&udp_addr,
                    sizeof(udp_addr)) < 0)
            {
                perror(
                    "UDP bind");


                close(
                    udp_sock);


                udp_sock =
                    -1;


                continue;
            }


            char monitor_command[128];


            snprintf(
                monitor_command,
                sizeof(monitor_command),
                "MONITOR START %d\n",
                UDP_PORT);


            send_all(
                sockfd,
                monitor_command,
                strlen(monitor_command));


            if (recv_line(
                    sockfd,
                    buffer,
                    sizeof(buffer)) <= 0)
            {
                close(
                    udp_sock);


                break;
            }


            printf(
                "Agent: %s\n",
                buffer);


            if (strncmp(
                    buffer,
                    "OK MONITOR_STARTED",
                    18) == 0)
            {
                udp_listener.sockfd =
                    udp_sock;


                udp_listener.active =
                    1;


                if (pthread_create(
                        &udp_thread,
                        NULL,
                        udp_listener_thread,
                        &udp_listener) == 0)
                {
                    udp_thread_started =
                        1;
                }
                else
                {
                    perror(
                        "pthread_create");


                    udp_listener.active =
                        0;


                    close(
                        udp_sock);


                    udp_sock =
                        -1;
                }
            }


            continue;
        }


        /* -------------------------------------------------
           MONITOR STOP
           ------------------------------------------------- */
        if (strcmp(
                command,
                "MONITOR STOP") == 0)
        {
            char stop_command[] =
                "MONITOR STOP\n";


            send_all(
                sockfd,
                stop_command,
                strlen(stop_command));


            if (recv_line(
                    sockfd,
                    buffer,
                    sizeof(buffer)) <= 0)
            {
                break;
            }


            printf(
                "Agent: %s\n",
                buffer);


            stop_udp_listener(
                &udp_listener,
                &udp_thread,
                &udp_thread_started);


            udp_sock =
                -1;


            continue;
        }


        /* -------------------------------------------------
           Other text commands
           ------------------------------------------------- */
        char protocol_command[BUFFER_SIZE];


        snprintf(
            protocol_command,
            sizeof(protocol_command),
            "%s\n",
            command);


        if (send_all(
                sockfd,
                protocol_command,
                strlen(protocol_command)) < 0)
        {
            break;
        }


        if (recv_line(
                sockfd,
                buffer,
                sizeof(buffer)) <= 0)
        {
            printf(
                "Agent disconnected.\n");


            break;
        }


        printf(
            "Agent: %s\n",
            buffer);


        if (strncmp(
                buffer,
                "OK BYE",
                6) == 0)
        {
            break;
        }
    }


    stop_udp_listener(
        &udp_listener,
        &udp_thread,
        &udp_thread_started);


    close(
        sockfd);


    printf(
        "Controller closed.\n");


    return 0;
}
