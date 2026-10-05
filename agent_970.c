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


void get_system_info(double *cpu_load,
                     long *memory_used_mb,
                     long *uptime_sec)
{
    FILE *fp;

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

            if (mem_total > 0 && mem_available > 0)
            {
                break;
            }
        }

        fclose(fp);
    }

    *memory_used_mb =
        (mem_total - mem_available) / 1024;

    double uptime;

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

        else if (strcmp(buffer,
                        "LISTPROC") == 0)
        {
            char process_list[4096];

            get_process_list(process_list,
                             sizeof(process_list));

            snprintf(response,
                     sizeof(response),
                     "OK PROCS %.850s %s\n",
                     process_list,
                     SID_TAG);

            send_all(client_fd,
                     response,
                     strlen(response));
        }

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
                snprintf(response,
                         sizeof(response),
                         "OK EXEC_RESULT %.700s %s\n",
                         exec_output,
                         SID_TAG);
            }
            else if (result == 0)
            {
                snprintf(response,
                         sizeof(response),
                         "ERR 002 COMMAND_NOT_ALLOWED %s\n",
                         SID_TAG);
            }
            else
            {
                snprintf(response,
                         sizeof(response),
                         "ERR 006 EXECUTION_FAILED %s\n",
                         SID_TAG);
            }

            send_all(client_fd,
                     response,
                     strlen(response));
        }

        else if (strcmp(buffer,
                        "QUIT") == 0)
        {
            snprintf(response,
                     sizeof(response),
                     "OK BYE %s\n",
                     SID_TAG);

            send_all(client_fd,
                     response,
                     strlen(response));

            printf("Controller requested QUIT.\n");

            break;
        }

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
