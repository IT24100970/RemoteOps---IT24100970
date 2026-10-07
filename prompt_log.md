# Prompt Log
## IE3090 Network Programming
## RemoteOps – IT24100970

**AI Tool:** ChatGPT 

"Add authentication using OPS-0970 and SID:0790."

**How the response was used:**  
Authentication was added as the first command after connection. Valid authentication returned an authenticated response, while invalid credentials returned an error message.

**Prompt:**  
"Add SYSINFO and allow multiple commands over one TCP connection."

**How the response was used:**  
A persistent command loop was added so the same connection could handle several commands. SYSINFO was implemented using Linux system files to obtain CPU load, memory usage, and uptime.

**Prompt:**  
"Add UDP periodic system monitoring to RemoteOps."

**How the response was used:**  
UDP monitoring was added. MONITOR START starts periodic system information messages, and MONITOR STOP stops the monitoring while keeping the TCP session active.

**Prompt:**  
"How to get timestamped logging."

**How the response was used:**  
A thread-safe logging system was added to the Agent. The log records connections, authentication, commands, file transfers, and disconnections in remoteops_IT24100970.log.
