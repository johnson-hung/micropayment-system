[Computer Network Programming Project]
(Linux socket programming in C/C++)
SSL over TCP/IP

Compile all the necessary files:
$ make server
$ make client

How-To-Use:

	[Server]
	To execute this program, you have to set port number (range: 1024-65535).

	-Format:
		./server<space><portNumber>

	-Example:
		To set up a server locally, and you want the portNumber to be 1024,
		you have to input:

		./server 1024
		
		If successfully set up, server will create 5 threads and waiting for 
		connections. (threadpool implemented)
		

	[Client]
	To execute this program, you have to set IP_address and port number for client.
	(same as server's IP_address and portNumber)

	-Format:
		./client<space><IPaddress><space><portNumber>

	-Example:
		You have set up a server locally, and server's portNumber is 1024.
		To create socket and make clients connect to the server, you have to input:
		
		./client 127.0.0.1 1024
		
		set IPaddress to localhost(127.0.0.1)
		set portNumber to 1024 
		
		if client successfully connected to server, a menu will show up:

		===[Micropayment System]===
		|                         |
		|      Input Command:     |
		|     'r' -- Register     |
		|     'i' -- Login        |
		|                         |
		===========================

		- To register an account, please input key 'r',then enter an username for
		  new account.

		- To login, please input key 'i', then fill in your account username and 
		  set a temporary port to get online.
		
		- When online, client is able to get account balance and list of online 
		  users by inputting "List". If client wants to exit, just type "Exit", then 
		  the socket will close.

		- New: <payer_username>#<payment>#<payee_username> to send money transaction
		  message to payee. 
		  Once the payee receive message, display:

		  	Incoming payment transaction:
		  	<payer_username>#<payment>#<payee_username>

		  transfer that msg to server, payee will get notification:
		
		  	(Transfer message to server)
			You got <payment> from <payer>
			Current account balance: <money>

		  (As a payer, call 'List' to make sure that payment is correctly transferred)
			



