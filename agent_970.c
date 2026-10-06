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

#define BUFFER_SIZE 1024

#define STORAGE_DIR "./agentfiles/IT24100970/"
#define MAX_FILE_SIZE (10 * 1024 * 1024)


/* --------------------------------------------------
   Receive one newline-terminated protocol line
   -------------------------------------------------- */
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


/* --------------------------------------------------
   Send all bytes in a buffer
   -------------------------------------------------- */
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


/* --------------------------------------------------
   Read CPU, memory and uptime
   -------------------------------------------------- */
void get_system_info(double *cpu_load,
                     long *memory_used_mb,
                     long *uptime_sec)
{
    FILE *fp;

    /* CPU load */
    fp = fopen("/proc/loadavg", "r");

    if (fp != NULL)
    {
        fscanf(fp, "%lf", cpu_load);
        fclose(fp);
    }
    else
    {
        *cpu_load = 0.0;
    }


    /* Memory */
    long mem_total = 0;
    long mem_available = 0;

    fp = fopen("/proc/meminfo", "r");

    if (fp != NULL)
    {
        char key[64];
        long value;
        char unit[32];

        while (fscanf(fp,
                      "%63s %ld %31s",
                      key,
                      &value,
                      unit) == 3)
        {
            if (strcmp(key, "MemTotal:") == 0)
            {
                mem_total = value;
            }
            else if (strcmp(key, "MemAvailable:") == 0)
            {
                mem_available = value;
            }

            if (mem_total > 0 &&
                mem_available > 0)
            {
                break;
            }
        }

        fclose(fp);
    }

    *memory_used_mb =
        (mem_total - mem_available) / 1024;


    /* Uptime */
    double uptime = 0;

    fp = fopen("/proc/uptime", "r");

    if (fp != NULL)
    {
        fscanf(fp, "%lf", &uptime);
        fclose(fp);

        *uptime_sec = (long)uptime;
    }
    else
    {
        *uptime_sec = 0;
    }
}


/* --------------------------------------------------
   Get running process list
   -------------------------------------------------- */
void get_process_list(char *output, int max_size)
{
    FILE *fp;

    output[0] = '\0';

    fp = popen("ps -eo pid,comm --no-headers", "r");

    if (fp == NULL)
    {
        snprintf(output,
                 max_size,
                 "PROCESS_LIST_UNAVAILABLE");

        return;
    }

    char line[128];

    while (fgets(line,
                 sizeof(line),
                 fp) != NULL)
    {
        line[strcspn(line, "\r\n")] = '\0';

        if ((int)(strlen(output) +
                  strlen(line) + 2) >= max_size)
        {
            break;
        }

        if (strlen(output) > 0)
        {
            strcat(output, ",");
        }

        strcat(output, line);
    }

    pclose(fp);
}


/* --------------------------------------------------
   Execute only the fixed whitelist
   -------------------------------------------------- */
int execute_whitelisted_command(const char *name,
                                char *output,
                                int max_size)
{
    const char *shell_command = NULL;

    if (strcmp(name, "DATE") == 0)
    {
        shell_command = "date";
    }
    else if (strcmp(name, "UPTIME") == 0)
    {
        shell_command = "uptime";
    }
    else if (strcmp(name, "DISKFREE") == 0)
    {
        shell_command = "df -h /";
    }
    else if (strcmp(name, "HOSTNAME") == 0)
    {
        shell_command = "hostname";
    }
    else if (strcmp(name, "WHOAMI") == 0)
    {
        shell_command = "whoami";
    }
    else
    {
        return 0;
    }

    FILE *fp = popen(shell_command, "r");

    if (fp == NULL)
    {
        snprintf(output,
                 max_size,
                 "EXECUTION_FAILED");

        return -1;
    }

    output[0] = '\0';

    char line[256];

    while (fgets(line,
                 sizeof(line),
                 fp) != NULL)
    {
        line[strcspn(line, "\r\n")] = ' ';

        if ((int)(strlen(output) +
                  strlen(line) + 1) >= max_size)
        {
            break;
        }

        strcat(output, line);
    }

    pclose(fp);

    return 1;
}


/* --------------------------------------------------
   Check uploaded/downloaded filename
   -------------------------------------------------- */
int is_safe_filename(const char *filename)
{
    if (filename == NULL ||
        strlen(filename) == 0)
    {
        return 0;
    }

    if (strstr(filename, "..") != NULL)
    {
        return 0;
    }

    if (strchr(filename, '/') != NULL)
    {
        return 0;
    }

    return 1;
}


/* --------------------------------------------------
   Receive exactly filesize raw bytes
   -------------------------------------------------- */
int receive_file_bytes(int sockfd,
                       FILE *fp,
                       long filesize)
{
    char file_buffer[4096];

    long total_received = 0;

    while (total_received < filesize)
    {
        long remaining =
            filesize - total_received;

        int to_receive;

        if (remaining < (long)sizeof(file_buffer))
        {
            to_receive = (int)remaining;
        }
        else
        {
            to_receive = sizeof(file_buffer);
        }

        int received =
            recv(sockfd,
                 file_buffer,
                 to_receive,
                 0);

        if (received <= 0)
        {
            return -1;
        }

        size_t written =
            fwrite(file_buffer,
                   1,
                   received,
                   fp);

        if (written != (size_t)received)
        {
            return -1;
        }

        total_received += received;
    }

    return 0;
}


/* --------------------------------------------------
   Send exactly filesize raw bytes
   -------------------------------------------------- */
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

        size_t to_read;

        if (remaining < (long)sizeof(file_buffer))
        {
            to_read = (size_t)remaining;
        }
        else
        {
            to_read = sizeof(file_buffer);
        }

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


/* --------------------------------------------------
   Main Agent
   -------------------------------------------------- */
int main(void)
{
    int server_fd;
    int client_fd;
    int opt = 1;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len =
        sizeof(client_addr);

    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];


    /* Create server TCP socket */
    server_fd =
        socket(AF_INET,
               SOCK_STREAM,
               0);

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


    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family =
        AF_INET;

    server_addr.sin_addr.s_addr =
        INADDR_ANY;

    server_addr.sin_port =
        htons(PORT);


    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }


    if (listen(server_fd,
               BACKLOG) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }


    printf("RemoteOps Agent listening on port %d...\n",
           PORT);


    /* Accept Controller */
    client_fd =
        accept(server_fd,
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


    /* ==================================================
       AUTHENTICATION
       ================================================== */

    int received =
        recv_line(client_fd,
                  buffer,
                  sizeof(buffer));

    if (received <= 0)
    {
        printf("Controller disconnected before authentication.\n");

        close(client_fd);
        close(server_fd);

        return 0;
    }


    printf("Received: %s\n",
           buffer);


    if (strcmp(buffer,
               "AUTH " AUTH_TOKEN) != 0)
    {
        snprintf(response,
                 sizeof(response),
                 "ERR 001 AUTH_FAILED %s\n",
                 SID_TAG);

        send_all(client_fd,
                 response,
                 strlen(response));

        printf("Authentication failed.\n");

        close(client_fd);
        close(server_fd);

        return 0;
    }


    snprintf(response,
             sizeof(response),
             "OK AUTHENTICATED %s\n",
             SID_TAG);

    send_all(client_fd,
             response,
             strlen(response));

    printf("Authentication successful.\n");


    /* ==================================================
       COMMAND LOOP
       ================================================== */

    while (1)
    {
        received =
            recv_line(client_fd,
                      buffer,
                      sizeof(buffer));

        if (received == 0)
        {
            printf("Controller disconnected.\n");
            break;
        }

        if (received < 0)
        {
            perror("recv");
            break;
        }


        printf("Received command: %s\n",
               buffer);


        /* --------------------------------------------------
           SYSINFO
           -------------------------------------------------- */
        if (strcmp(buffer,
                   "SYSINFO") == 0)
        {
            double cpu_load;
            long memory_used_mb;
            long uptime_sec;

            get_system_info(
                &cpu_load,
                &memory_used_mb,
                &uptime_sec);

            snprintf(
                response,
                sizeof(response),
                "OK SYSINFO %.2f %ld %ld %s\n",
                cpu_load,
                memory_used_mb,
                uptime_sec,
                SID_TAG);

            send_all(client_fd,
                     response,
                     strlen(response));
        }


        /* --------------------------------------------------
           LISTPROC
           -------------------------------------------------- */
        else if (strcmp(buffer,
                        "LISTPROC") == 0)
        {
            char process_list[4096];

            get_process_list(
                process_list,
                sizeof(process_list));

            snprintf(
                response,
                sizeof(response),
                "OK PROCS %.850s %s\n",
                process_list,
                SID_TAG);

            send_all(client_fd,
                     response,
                     strlen(response));
        }


        /* --------------------------------------------------
           EXEC
           -------------------------------------------------- */
        else if (strncmp(buffer,
                         "EXEC ",
                         5) == 0)
        {
            const char *command_name =
                buffer + 5;

            char exec_output[768];

            int result =
                execute_whitelisted_command(
                    command_name,
                    exec_output,
                    sizeof(exec_output));


            if (result == 1)
            {
                snprintf(
                    response,
                    sizeof(response),
                    "OK EXEC_RESULT %.700s %s\n",
                    exec_output,
                    SID_TAG);
            }

            else if (result == 0)
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 002 COMMAND_NOT_ALLOWED %s\n",
                    SID_TAG);
            }

            else
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 006 EXECUTION_FAILED %s\n",
                    SID_TAG);
            }


            send_all(client_fd,
                     response,
                     strlen(response));
        }


        /* --------------------------------------------------
           PUT - File Upload
           -------------------------------------------------- */
        else if (strncmp(buffer,
                         "PUT ",
                         4) == 0)
        {
            char filename[256];
            long filesize;


            int parsed =
                sscanf(buffer,
                       "PUT %255s %ld",
                       filename,
                       &filesize);


            if (parsed != 2 ||
                filesize < 0)
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 007 INVALID_PUT_REQUEST %s\n",
                    SID_TAG);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }


            if (!is_safe_filename(filename))
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 008 INVALID_FILENAME %s\n",
                    SID_TAG);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }


            if (filesize > MAX_FILE_SIZE)
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 004 FILE_TOO_LARGE %s\n",
                    SID_TAG);

                send_all(client_fd,
                         response,
                         strlen(response));

                break;
            }


            char filepath[512];

            snprintf(filepath,
                     sizeof(filepath),
                     "%s%s",
                     STORAGE_DIR,
                     filename);


            FILE *fp =
                fopen(filepath,
                      "wb");


            if (fp == NULL)
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 009 FILE_WRITE_ERROR %s\n",
                    SID_TAG);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }


            printf("Receiving file: %s (%ld bytes)\n",
                   filename,
                   filesize);


            if (receive_file_bytes(
                    client_fd,
                    fp,
                    filesize) < 0)
            {
                fclose(fp);

                remove(filepath);

                printf("File transfer failed.\n");

                break;
            }


            fclose(fp);


            snprintf(
                response,
                sizeof(response),
                "OK FILE_RECEIVED %s %s\n",
                filename,
                SID_TAG);


            send_all(client_fd,
                     response,
                     strlen(response));


            printf("File received successfully: %s\n",
                   filepath);
        }


        /* --------------------------------------------------
           GET - File Download
           -------------------------------------------------- */
        else if (strncmp(buffer,
                         "GET ",
                         4) == 0)
        {
            char filename[256];


            if (sscanf(buffer,
                       "GET %255s",
                       filename) != 1)
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 010 INVALID_GET_REQUEST %s\n",
                    SID_TAG);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }


            if (!is_safe_filename(filename))
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 008 INVALID_FILENAME %s\n",
                    SID_TAG);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }


            char filepath[512];

            snprintf(filepath,
                     sizeof(filepath),
                     "%s%s",
                     STORAGE_DIR,
                     filename);


            FILE *fp =
                fopen(filepath,
                      "rb");


            if (fp == NULL)
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 005 FILE_NOT_FOUND %s\n",
                    SID_TAG);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }


            if (fseek(fp,
                      0,
                      SEEK_END) != 0)
            {
                fclose(fp);

                snprintf(
                    response,
                    sizeof(response),
                    "ERR 011 FILE_READ_ERROR %s\n",
                    SID_TAG);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }


            long filesize =
                ftell(fp);


            if (filesize < 0)
            {
                fclose(fp);

                snprintf(
                    response,
                    sizeof(response),
                    "ERR 011 FILE_READ_ERROR %s\n",
                    SID_TAG);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }


            rewind(fp);


            snprintf(
                response,
                sizeof(response),
                "OK FILE_SEND %s %ld %s\n",
                filename,
                filesize,
                SID_TAG);


            if (send_all(client_fd,
                         response,
                         strlen(response)) < 0)
            {
                fclose(fp);
                break;
            }


            printf("Sending file: %s (%ld bytes)\n",
                   filename,
                   filesize);


            if (send_file_bytes(
                    client_fd,
                    fp,
                    filesize) < 0)
            {
                printf("File download transfer failed.\n");

                fclose(fp);

                break;
            }


            fclose(fp);


            printf("File sent successfully: %s\n",
                   filepath);
        }


        /* --------------------------------------------------
           QUIT
           -------------------------------------------------- */
        else if (strcmp(buffer,
                        "QUIT") == 0)
        {
            snprintf(
                response,
                sizeof(response),
                "OK BYE %s\n",
                SID_TAG);


            send_all(client_fd,
                     response,
                     strlen(response));


            printf("Controller requested QUIT.\n");

            break;
        }


        /* --------------------------------------------------
           Unknown command
           -------------------------------------------------- */
        else
        {
            snprintf(
                response,
                sizeof(response),
                "ERR 003 UNKNOWN_COMMAND %s\n",
                SID_TAG);


            send_all(client_fd,
                     response,
                     strlen(response));
        }
    }


    close(client_fd);
    close(server_fd);

    printf("Connection closed.\n");

    return 0;
}
