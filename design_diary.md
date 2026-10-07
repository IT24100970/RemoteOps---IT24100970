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


## Step 7 - Restricted EXEC Command
Date: 06 Oct 2026

I implemented the EXEC command using the fixed whitelist required by the assignment.

Supported commands:
- DATE
- UPTIME
- DISKFREE
- HOSTNAME
- WHOAMI

Each protocol command is mapped to a fixed Linux shell command.

Valid requests return:

OK EXEC_RESULT <output> SID:0790

Any command outside the whitelist returns:

ERR 002 COMMAND_NOT_ALLOWED SID:0790

Testing:
I successfully tested all five approved EXEC commands.

I also tested rejected commands such as:

EXEC LS
EXEC RM

Both were rejected correctly.

Decision:
I used a fixed mapping instead of executing arbitrary Controller input because unrestricted remote command execution would create a serious security risk and would violate the assignment specification.


## Step 8 - PUT File Upload
Date 06 Oct 2026

I implemented the PUT command to allow the Controller to upload files to the Agent.

The Controller opens the requested local file, calculates its size and sends:

PUT <test.txt> <42 bytes>

followed immediately by the raw file bytes.

The Agent receives exactly the declared number of bytes and stores the file under:  ./agentfiles/IT24100970/

A successful transfer returns:

OK FILE_RECEIVED <filename> SID:0790

I implemented a loop for file transfer because TCP is stream-oriented and one recv() call is not guaranteed to return the complete file.

Filename validation was also added to prevent uploaded files from escaping the personalised storage directory.
A maximum file size of 10 MB was selected as an implementation assumption.

Testing:
I uploaded test.txt and confirmed that it appeared inside the personalised storage directory. I used cmp / SHA-256 verification to confirm that the stored file was identical to the original.

## Step 9 - GET File Download
Date 06 Oct 2026

I implemented the GET command so that the Controller can retrieve a previously uploaded file from the Agent.

The Controller sends:

GET <tst.txt>

If the file exists, the Agent responds with:

OK FILE_SEND <test.txt> <42 bytes> SID:0790

The Agent then sends exactly the specified number of raw file bytes.

The Controller saves the received file using a downloaded_ prefix.

Testing:
I downloaded test.txt and saved it as downloaded_test.txt.

I used cmp and SHA-256 hashes to verify that the downloaded file was byte-for-byte identical to the original.

I also tested a missing file request and confirmed that the Agent returned:

ERR 005 FILE_NOT_FOUND SID:0790


## Step 10 - UDP Periodic Monitoring
Date 06 Oct 2026

I implemented periodic UDP monitoring using MONITOR START and MONITOR STOP.

The Controller opens a UDP socket on port 9500 and sends:

MONITOR START 9500

The Agent starts a separate monitoring thread and periodically sends system statistics to the Controller using UDP.

The datagram format is:

SYSINFO <0.15> <2540> <92978> SID:0790

A monitoring interval of 3 seconds was selected.

MONITOR STOP terminates the monitoring thread and stops further UDP datagrams.

Testing:
I successfully received multiple UDP SYSINFO datagrams while the TCP control connection remained active. After MONITOR STOP, the periodic messages stopped.


## Step 12 - Timestamped Logging
Date 07 Oct 2026

I implemented timestamped logging in the RemoteOps Agent.

The Agent writes operational events to:

remoteops_IT24100970.log

Logged events include:
- Controller connections
- Authentication success and failure
- Commands
- PUT file transfers
- GET file transfers
- Controller disconnects

Each log entry contains a timestamp.

A pthread mutex was used to protect the log file because multiple Controller threads may write to the log at the same time.

Testing:
I executed SYSINFO, LISTPROC, EXEC, PUT, GET and QUIT, then verified that the corresponding timestamped records were written to the personalised log file.


## Step 13 - Makefile and Final Regression Testing
Date 07 Oct 2026

I completed the personalised Makefile and performed full regression testing of the RemoteOps implementation.

The Makefile compiles both:
- agent_970.c
- controller_970.c

using gcc with pthread support.

Final tests covered:
- Authentication
- SYSINFO
- LISTPROC
- All five allowed EXEC commands
- Rejected EXEC command
- PUT file upload
- GET file download
- Missing-file error
- UDP monitoring start and stop
- QUIT
- Timestamped logging
- Five simultaneous Controller connections

File integrity was verified using cmp and SHA-256 hashes.

The Agent was also verified to be listening on the personalised TCP port 9410.
