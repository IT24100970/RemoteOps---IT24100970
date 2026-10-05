# RemoteOps Design Diary
Registration ID: IT24100970

## Step 1 - Project Setup
Date: 05 Oct 2026

I created the RemoteOps project structure using the personalised filenames required by the assignment.

Files created:
- agent_970.c
- controller_970.c
- Makefile_970
- README.md
- design_diary.md
- prompt_log.md

I also created the personalized storage directory:

./agentfiles/IT24100970/

Git was initialized in the RemoteOps directory and connected to GitHub using SSH authentication.

Personalised values used:
- TCP Port: 9410
- SID: 0790
- Authentication Token: OPS-0970
- Log File: remoteops_IT24100970.log

Decision:
I decided to develop the project incrementally and create meaningful Git commits after each completed and tested feature.

Obstacle:
GitHub HTTPS authentication initially failed because password authentication is not supported. I solved this by configuring SSH authentication and successfully pushed the repository to GitHub.

## Step 2 - TCP Agent Server
Date: 05 Oct 2026

I started implementing the RemoteOps Agent in C using the BSD socket API.

The Agent currently:
- Creates an IPv4 TCP socket using socket().
- Uses SO_REUSEADDR so the server port can be reused after restarting.
- Binds to the personalized TCP port 9410.
- Listens for incoming Controller connections.
- Accepts a basic TCP client connection.

Decision:
I used TCP because the RemoteOps control channel requires reliable and ordered communication.

## Step 3 - TCP Controller CLient
date 05 Oct 2026

I implemented the initial Controller Client

Contriller:
- Creates IPv4 TCP socket using socket()
- Configures agent IP address as 127.0.0.1.
- Uses personalized Agent TCP port 9410
- Connects to the Agent using connect().
- Displays confirmation when TCP connection is established successfully.
- Closes the socket cleanlt after test.

Testing:
I started agent_970 in one terminal & controller_970 in another yerminal.

Controller successfully connected Agent on:
127.0.0.1:9410
Agent also displayed controller IP address & source port.

## Step 4 - Authentication
Date 06 Oct 2026

I implemented the AUTH command required by the RemoteOps protocol.

The Controller sends:  AUTH OPS-0970

The Agent checks the received token against the personalised authentication token.

Successful authentication returns:   OK AUTHENTICATED SID:0790

An invalid token returns:   ERR 001 AUTH_FAILED SID:0790

Testing:
I tested both correct and incorrect authentication tokens.

The valid token OPS-0970 was accepted successfully.
An incorrect token was rejected with AUTH_FAILED.

Decision:
Authentication is implemented before other RemoteOps commands because the protocol requires AUTH to be the first command on a new connection.


## Step 5 - Persistent Command Loop and SYSINFO
Date 06 Oct 2026

I extended the RemoteOps Agent and Controller so that an authenticated Controller can remain connected and issue multiple commands.

A recv_line() function was introduced to read newline-terminated protocol commands. This was necessary because TCP is stream-oriented and a single recv() call cannot be assumed to contain exactly one complete command.

A send_all() function was also introduced to ensure that complete responses are transmitted.

SYSINFO Implementation:
The Agent reads Linux system information from:
- /proc/loadavg for CPU load
- /proc/meminfo for memory usage
- /proc/uptime for system uptime

The response follows the required format:
OK SYSINFO <cpu_load> <mem_used_mb> <uptime_sec> SID:0790

I also implemented QUIT, which returns:   OK BYE SID:0790

Testing:
I successfully authenticated, executed SYSINFO multiple times within the same TCP session, tested an unsupported command, and used QUIT to close the connection cleanly.

Decision:
Linux /proc files were used because they provide current system information without requiring external libraries.

## Step 6 - LISTPROC Command
Date 06 Oct 2026

I implemented the LISTPROC command in the RemoteOps Agent.

The Agent uses popen() to execute:   ps -eo pid,comm --no-headers

The output is read line by line and converted into a comma-separated process list.

The protocol response follows the required format:

OK PROCS <process list> SID:0790

Testing:
I authenticated using the Controller and executed LISTPROC successfully. The Agent returned a snapshot of currently running Linux processes.

I also tested SYSINFO, LISTPROC and QUIT within the same authenticated TCP connection.

Decision:
I used popen() because it allows the Agent to execute the Linux ps command and capture the output directly for inclusion in the protocol response.
