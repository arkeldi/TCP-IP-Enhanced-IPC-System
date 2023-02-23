# <p align="center">The Server Moved Out!<p>

**Introduction**

TCPRequestChannel is a classes added to extend the IPC capabilities of the client-server implementation using the TCP/IP protocol. The client-side and server-side ends of a TCPRequestChannel will reside on different machines.

Since the communication API (not just the underlying functionality and features) through TCP/IP is different from FIFO, I also needed to restructure the server.cpp and client.cpp:

- The server program must be modified to handle incoming requests across the network request channels using the TCP/IP protocol. Specifically, the server must be able to handle multiple request channels from the client residing on a different machine. 

- I also had to modify the client to send requests across the network request channels using the TCP/IP protocol.

The code analysis document(pdf) I have attached go into depth of the program. 
  
**Tasks**

- [ ] Implement the TCPRequestChannel class
  - [ ] write the TCPRequestChannel(string, string) and TCPRequestChannel(int) constructors
  - [ ] write the destructor
  - [ ] write accept_conn() so the server can accept client connections
  - [ ] write cread(void*, int) and cwrite(void*, int) methods
- [ ] Modify client
  - [ ] add a and r options
  - [ ] change request channel instances to TCPRequestChannel
- [ ] Modify server
  - [ ] add r option
  - [ ] implement server's primary channel
  - [ ] implement server's accept loop
