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

