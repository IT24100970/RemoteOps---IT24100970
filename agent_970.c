#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 9410
#define BACKLOG 10

#define AUTH_TOKEN "OPS-0970"
#define SID_TAG "SID:0790"

#define BUFFER_SIZE 1024

#define STORAGE_DIR "./agentfiles/IT24100970/"
#define MAX_FILE_SIZE (10 * 1024 * 1024)

#define MONITOR_INTERVAL 3


/* =========================================================
   UDP monitoring information for one Controller session
   ========================================================= */
typedef struct
{
    volatile int active;
    int thread_started;

    int udp_port;

    char client_ip[INET_ADDRSTRLEN];

    pthread_t thread;

} monitor_session_t;


/* =========================================================
   Information passed to each client thread
   ========================================================= */
typedef struct
{
    int client_fd;

    struct sockaddr_in client_addr;

} client_session_t;


/* =========================================================
   Receive one newline-terminated TCP command
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
   Send all bytes
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
   Obtain CPU load, memory usage and uptime
   ========================================================= */
void get_system_info(double *cpu_load,
                     long *memory_used_mb,
                     long *uptime_sec)
{
    FILE *fp;


    /* CPU LOAD */
    fp =
        fopen(
            "/proc/loadavg",
            "r");


    if (fp != NULL)
    {
        if (fscanf(
                fp,
                "%lf",
                cpu_load) != 1)
        {
            *cpu_load = 0.0;
        }


        fclose(fp);
    }
    else
    {
        *cpu_load = 0.0;
    }


    /* MEMORY */
    long mem_total = 0;
    long mem_available = 0;


    fp =
        fopen(
            "/proc/meminfo",
            "r");


    if (fp != NULL)
    {
        char line[256];


        while (fgets(
                   line,
                   sizeof(line),
                   fp) != NULL)
        {
            sscanf(
                line,
                "MemTotal: %ld kB",
                &mem_total);


            sscanf(
                line,
                "MemAvailable: %ld kB",
                &mem_available);
        }


        fclose(fp);
    }


    if (mem_total >= mem_available)
    {
        *memory_used_mb =
            (mem_total - mem_available) / 1024;
    }
    else
    {
        *memory_used_mb = 0;
    }


    /* UPTIME */
    double uptime = 0;


    fp =
        fopen(
            "/proc/uptime",
            "r");


    if (fp != NULL)
    {
        if (fscanf(
                fp,
                "%lf",
                &uptime) == 1)
        {
            *uptime_sec =
                (long)uptime;
        }
        else
        {
            *uptime_sec = 0;
        }


        fclose(fp);
    }
    else
    {
        *uptime_sec = 0;
    }
}


/* =========================================================
   Obtain process list
   ========================================================= */
void get_process_list(char *output,
                      int max_size)
{
    FILE *fp;


    output[0] = '\0';


    fp =
        popen(
            "ps -eo pid,comm --no-headers",
            "r");


    if (fp == NULL)
    {
        snprintf(
            output,
            max_size,
            "PROCESS_LIST_UNAVAILABLE");


        return;
    }


    char line[128];


    while (fgets(
               line,
               sizeof(line),
               fp) != NULL)
    {
        line[
            strcspn(
                line,
                "\r\n")
        ] = '\0';


        int required =
            strlen(output)
            +
            strlen(line)
            +
            2;


        if (required >= max_size)
        {
            break;
        }


        if (strlen(output) > 0)
        {
            strcat(
                output,
                ",");
        }


        strcat(
            output,
            line);
    }


    pclose(fp);
}


/* =========================================================
   Execute only allowed commands
   ========================================================= */
int execute_whitelisted_command(const char *name,
                                char *output,
                                int max_size)
{
    const char *shell_command =
        NULL;


    if (strcmp(
            name,
            "DATE") == 0)
    {
        shell_command =
            "date";
    }

    else if (strcmp(
                 name,
                 "UPTIME") == 0)
    {
        shell_command =
            "uptime";
    }

    else if (strcmp(
                 name,
                 "DISKFREE") == 0)
    {
        shell_command =
            "df -h /";
    }

    else if (strcmp(
                 name,
                 "HOSTNAME") == 0)
    {
        shell_command =
            "hostname";
    }

    else if (strcmp(
                 name,
                 "WHOAMI") == 0)
    {
        shell_command =
            "whoami";
    }

    else
    {
        return 0;
    }


    FILE *fp =
        popen(
            shell_command,
            "r");


    if (fp == NULL)
    {
        snprintf(
            output,
            max_size,
            "EXECUTION_FAILED");


        return -1;
    }


    output[0] = '\0';


    char line[256];


    while (fgets(
               line,
               sizeof(line),
               fp) != NULL)
    {
        line[
            strcspn(
                line,
                "\r\n")
        ] = '\0';


        int required =
            strlen(output)
            +
            strlen(line)
            +
            2;


        if (required >= max_size)
        {
            break;
        }


        if (strlen(output) > 0)
        {
            strcat(
                output,
                " ");
        }


        strcat(
            output,
            line);
    }


    pclose(fp);


    return 1;
}


/* =========================================================
   Validate filename
   ========================================================= */
int is_safe_filename(const char *filename)
{
    if (filename == NULL ||
        strlen(filename) == 0)
    {
        return 0;
    }


    if (strstr(
            filename,
            "..") != NULL)
    {
        return 0;
    }


    if (strchr(
            filename,
            '/') != NULL)
    {
        return 0;
    }


    return 1;
}


/* =========================================================
   Receive file bytes
   ========================================================= */
int receive_file_bytes(int sockfd,
                       FILE *fp,
                       long filesize)
{
    char file_buffer[4096];


    long total_received =
        0;


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
   Send file bytes
   ========================================================= */
int send_file_bytes(int sockfd,
                    FILE *fp,
                    long filesize)
{
    char file_buffer[4096];


    long total_sent =
        0;


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
   UDP monitoring thread
   ========================================================= */
void *monitor_thread(void *arg)
{
    monitor_session_t *session =
        (monitor_session_t *)arg;


    int udp_sock =
        socket(
            AF_INET,
            SOCK_DGRAM,
            0);


    if (udp_sock < 0)
    {
        perror(
            "UDP socket");


        session->active =
            0;


        return NULL;
    }


    struct sockaddr_in udp_addr;


    memset(
        &udp_addr,
        0,
        sizeof(udp_addr));


    udp_addr.sin_family =
        AF_INET;


    udp_addr.sin_port =
        htons(
            session->udp_port);


    if (inet_pton(
            AF_INET,
            session->client_ip,
            &udp_addr.sin_addr) <= 0)
    {
        perror(
            "UDP inet_pton");


        close(
            udp_sock);


        session->active =
            0;


        return NULL;
    }


    printf(
        "UDP monitoring started for %s:%d\n",
        session->client_ip,
        session->udp_port);


    while (session->active)
    {
        double cpu_load;

        long memory_used_mb;

        long uptime_sec;


        get_system_info(
            &cpu_load,
            &memory_used_mb,
            &uptime_sec);


        char message[256];


        snprintf(
            message,
            sizeof(message),
            "SYSINFO %.2f %ld %ld %s",
            cpu_load,
            memory_used_mb,
            uptime_sec,
            SID_TAG);


        sendto(
            udp_sock,
            message,
            strlen(message),
            0,
            (struct sockaddr *)&udp_addr,
            sizeof(udp_addr));


        for (int i = 0;
             i < MONITOR_INTERVAL;
             i++)
        {
            if (!session->active)
            {
                break;
            }


            sleep(1);
        }
    }


    close(
        udp_sock);


    printf(
        "UDP monitoring stopped for %s\n",
        session->client_ip);


    return NULL;
}


/* =========================================================
   Stop monitoring thread safely
   ========================================================= */
void stop_monitoring(monitor_session_t *monitor)
{
    if (monitor->thread_started)
    {
        monitor->active =
            0;


        pthread_join(
            monitor->thread,
            NULL);


        monitor->thread_started =
            0;
    }
}


/* =========================================================
   Handle one connected Controller
   ========================================================= */
void *handle_client(void *arg)
{
    client_session_t *session =
        (client_session_t *)arg;


    int client_fd =
        session->client_fd;


    struct sockaddr_in client_addr =
        session->client_addr;


    free(
        session);


    char buffer[BUFFER_SIZE];

    char response[BUFFER_SIZE];


    monitor_session_t monitor;


    memset(
        &monitor,
        0,
        sizeof(monitor));


    inet_ntop(
        AF_INET,
        &client_addr.sin_addr,
        monitor.client_ip,
        sizeof(monitor.client_ip));


    printf(
        "Controller connected from %s:%d\n",
        inet_ntoa(
            client_addr.sin_addr),
        ntohs(
            client_addr.sin_port));


    /* =====================================================
       AUTHENTICATION
       ===================================================== */
    int received =
        recv_line(
            client_fd,
            buffer,
            sizeof(buffer));


    if (received <= 0)
    {
        printf(
            "Controller disconnected before authentication.\n");


        close(
            client_fd);


        return NULL;
    }


    printf(
        "Received: %s\n",
        buffer);


    if (strcmp(
            buffer,
            "AUTH " AUTH_TOKEN) != 0)
    {
        snprintf(
            response,
            sizeof(response),
            "ERR 001 AUTH_FAILED %s\n",
            SID_TAG);


        send_all(
            client_fd,
            response,
            strlen(response));


        printf(
            "Authentication failed.\n");


        close(
            client_fd);


        return NULL;
    }


    snprintf(
        response,
        sizeof(response),
        "OK AUTHENTICATED %s\n",
        SID_TAG);


    send_all(
        client_fd,
        response,
        strlen(response));


    printf(
        "Authentication successful.\n");


    /* =====================================================
       COMMAND LOOP
       ===================================================== */
    while (1)
    {
        received =
            recv_line(
                client_fd,
                buffer,
                sizeof(buffer));


        if (received == 0)
        {
            printf(
                "Controller disconnected.\n");


            break;
        }


        if (received < 0)
        {
            perror(
                "recv");


            break;
        }


        printf(
            "Received command: %s\n",
            buffer);


        /* -------------------------------------------------
           SYSINFO
           ------------------------------------------------- */
        if (strcmp(
                buffer,
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


            send_all(
                client_fd,
                response,
                strlen(response));
        }


        /* -------------------------------------------------
           LISTPROC
           ------------------------------------------------- */
        else if (strcmp(
                     buffer,
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


            send_all(
                client_fd,
                response,
                strlen(response));
        }


        /* -------------------------------------------------
           EXEC
           ------------------------------------------------- */
        else if (strncmp(
                     buffer,
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


            send_all(
                client_fd,
                response,
                strlen(response));
        }


        /* -------------------------------------------------
           PUT
           ------------------------------------------------- */
        else if (strncmp(
                     buffer,
                     "PUT ",
                     4) == 0)
        {
            char filename[256];

            long filesize;


            int parsed =
                sscanf(
                    buffer,
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


                send_all(
                    client_fd,
                    response,
                    strlen(response));


                continue;
            }


            if (!is_safe_filename(
                    filename))
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 008 INVALID_FILENAME %s\n",
                    SID_TAG);


                send_all(
                    client_fd,
                    response,
                    strlen(response));


                continue;
            }


            if (filesize >
                MAX_FILE_SIZE)
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 004 FILE_TOO_LARGE %s\n",
                    SID_TAG);


                send_all(
                    client_fd,
                    response,
                    strlen(response));


                break;
            }


            char filepath[512];


            snprintf(
                filepath,
                sizeof(filepath),
                "%s%s",
                STORAGE_DIR,
                filename);


            FILE *fp =
                fopen(
                    filepath,
                    "wb");


            if (fp == NULL)
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 009 FILE_WRITE_ERROR %s\n",
                    SID_TAG);


                send_all(
                    client_fd,
                    response,
                    strlen(response));


                continue;
            }


            printf(
                "Receiving file: %s (%ld bytes)\n",
                filename,
                filesize);


            if (receive_file_bytes(
                    client_fd,
                    fp,
                    filesize) < 0)
            {
                fclose(
                    fp);


                remove(
                    filepath);


                printf(
                    "File transfer failed.\n");


                break;
            }


            fclose(
                fp);


            snprintf(
                response,
                sizeof(response),
                "OK FILE_RECEIVED %s %s\n",
                filename,
                SID_TAG);


            send_all(
                client_fd,
                response,
                strlen(response));


            printf(
                "File received successfully: %s\n",
                filepath);
        }


        /* -------------------------------------------------
           GET
           ------------------------------------------------- */
        else if (strncmp(
                     buffer,
                     "GET ",
                     4) == 0)
        {
            char filename[256];


            if (sscanf(
                    buffer,
                    "GET %255s",
                    filename) != 1)
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 010 INVALID_GET_REQUEST %s\n",
                    SID_TAG);


                send_all(
                    client_fd,
                    response,
                    strlen(response));


                continue;
            }


            if (!is_safe_filename(
                    filename))
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 008 INVALID_FILENAME %s\n",
                    SID_TAG);


                send_all(
                    client_fd,
                    response,
                    strlen(response));


                continue;
            }


            char filepath[512];


            snprintf(
                filepath,
                sizeof(filepath),
                "%s%s",
                STORAGE_DIR,
                filename);


            FILE *fp =
                fopen(
                    filepath,
                    "rb");


            if (fp == NULL)
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 005 FILE_NOT_FOUND %s\n",
                    SID_TAG);


                send_all(
                    client_fd,
                    response,
                    strlen(response));


                continue;
            }


            if (fseek(
                    fp,
                    0,
                    SEEK_END) != 0)
            {
                fclose(
                    fp);


                snprintf(
                    response,
                    sizeof(response),
                    "ERR 011 FILE_READ_ERROR %s\n",
                    SID_TAG);


                send_all(
                    client_fd,
                    response,
                    strlen(response));


                continue;
            }


            long filesize =
                ftell(
                    fp);


            if (filesize < 0)
            {
                fclose(
                    fp);


                snprintf(
                    response,
                    sizeof(response),
                    "ERR 011 FILE_READ_ERROR %s\n",
                    SID_TAG);


                send_all(
                    client_fd,
                    response,
                    strlen(response));


                continue;
            }


            rewind(
                fp);


            snprintf(
                response,
                sizeof(response),
                "OK FILE_SEND %s %ld %s\n",
                filename,
                filesize,
                SID_TAG);


            if (send_all(
                    client_fd,
                    response,
                    strlen(response)) < 0)
            {
                fclose(
                    fp);


                break;
            }


            printf(
                "Sending file: %s (%ld bytes)\n",
                filename,
                filesize);


            if (send_file_bytes(
                    client_fd,
                    fp,
                    filesize) < 0)
            {
                fclose(
                    fp);


                printf(
                    "File download transfer failed.\n");


                break;
            }


            fclose(
                fp);


            printf(
                "File sent successfully: %s\n",
                filepath);
        }


        /* -------------------------------------------------
           MONITOR START
           ------------------------------------------------- */
        else if (strncmp(
                     buffer,
                     "MONITOR START ",
                     14) == 0)
        {
            int udp_port;


            if (sscanf(
                    buffer,
                    "MONITOR START %d",
                    &udp_port) != 1 ||
                udp_port <= 0 ||
                udp_port > 65535)
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 012 INVALID_UDP_PORT %s\n",
                    SID_TAG);


                send_all(
                    client_fd,
                    response,
                    strlen(response));


                continue;
            }


            stop_monitoring(
                &monitor);


            monitor.udp_port =
                udp_port;


            monitor.active =
                1;


            if (pthread_create(
                    &monitor.thread,
                    NULL,
                    monitor_thread,
                    &monitor) != 0)
            {
                monitor.active =
                    0;


                snprintf(
                    response,
                    sizeof(response),
                    "ERR 013 MONITOR_START_FAILED %s\n",
                    SID_TAG);


                send_all(
                    client_fd,
                    response,
                    strlen(response));


                continue;
            }


            monitor.thread_started =
                1;


            snprintf(
                response,
                sizeof(response),
                "OK MONITOR_STARTED %s\n",
                SID_TAG);


            send_all(
                client_fd,
                response,
                strlen(response));


            printf(
                "Monitoring started for Controller %s\n",
                monitor.client_ip);
        }


        /* -------------------------------------------------
           MONITOR STOP
           ------------------------------------------------- */
        else if (strcmp(
                     buffer,
                     "MONITOR STOP") == 0)
        {
            stop_monitoring(
                &monitor);


            snprintf(
                response,
                sizeof(response),
                "OK MONITOR_STOPPED %s\n",
                SID_TAG);


            send_all(
                client_fd,
                response,
                strlen(response));


            printf(
                "Monitoring stopped for Controller %s\n",
                monitor.client_ip);
        }


        /* -------------------------------------------------
           QUIT
           ------------------------------------------------- */
        else if (strcmp(
                     buffer,
                     "QUIT") == 0)
        {
            stop_monitoring(
                &monitor);


            snprintf(
                response,
                sizeof(response),
                "OK BYE %s\n",
                SID_TAG);


            send_all(
                client_fd,
                response,
                strlen(response));


            printf(
                "Controller requested QUIT.\n");


            break;
        }


        /* -------------------------------------------------
           UNKNOWN COMMAND
           ------------------------------------------------- */
        else
        {
            snprintf(
                response,
                sizeof(response),
                "ERR 003 UNKNOWN_COMMAND %s\n",
                SID_TAG);


            send_all(
                client_fd,
                response,
                strlen(response));
        }
    }


    stop_monitoring(
        &monitor);


    close(
        client_fd);


    printf(
        "Controller session closed: %s:%d\n",
        inet_ntoa(
            client_addr.sin_addr),
        ntohs(
            client_addr.sin_port));


    return NULL;
}


/* =========================================================
   MAIN SERVER
   ========================================================= */
int main(void)
{
    int server_fd;

    int opt = 1;


    struct sockaddr_in server_addr;


    /* -----------------------------------------------------
       Create TCP socket
       ----------------------------------------------------- */
    server_fd =
        socket(
            AF_INET,
            SOCK_STREAM,
            0);


    if (server_fd < 0)
    {
        perror(
            "socket");


        return 1;
    }


    /* -----------------------------------------------------
       Allow address reuse
       ----------------------------------------------------- */
    if (setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)) < 0)
    {
        perror(
            "setsockopt");


        close(
            server_fd);


        return 1;
    }


    memset(
        &server_addr,
        0,
        sizeof(server_addr));


    server_addr.sin_family =
        AF_INET;


    server_addr.sin_addr.s_addr =
        INADDR_ANY;


    server_addr.sin_port =
        htons(PORT);


    /* -----------------------------------------------------
       Bind
       ----------------------------------------------------- */
    if (bind(
            server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)) < 0)
    {
        perror(
            "bind");


        close(
            server_fd);


        return 1;
    }


    /* -----------------------------------------------------
       Listen
       ----------------------------------------------------- */
    if (listen(
            server_fd,
            BACKLOG) < 0)
    {
        perror(
            "listen");


        close(
            server_fd);


        return 1;
    }


    printf(
        "RemoteOps Agent listening on port %d...\n",
        PORT);


    printf(
        "Multi-client mode enabled.\n");


    /* =====================================================
       ACCEPT LOOP
       ===================================================== */
    while (1)
    {
        struct sockaddr_in client_addr;


        socklen_t client_len =
            sizeof(client_addr);


        int client_fd =
            accept(
                server_fd,
                (struct sockaddr *)&client_addr,
                &client_len);


        if (client_fd < 0)
        {
            perror(
                "accept");


            continue;
        }


        /* Allocate session data */
        client_session_t *session =
            malloc(
                sizeof(client_session_t));


        if (session == NULL)
        {
            perror(
                "malloc");


            close(
                client_fd);


            continue;
        }


        session->client_fd =
            client_fd;


        session->client_addr =
            client_addr;


        pthread_t client_thread;


        /* Create one thread for this Controller */
        if (pthread_create(
                &client_thread,
                NULL,
                handle_client,
                session) != 0)
        {
            perror(
                "pthread_create");


            close(
                client_fd);


            free(
                session);


            continue;
        }


        /*
         * Thread cleans up automatically when it finishes.
         */
        pthread_detach(
            client_thread);
    }


    close(
        server_fd);


    return 0;
}
