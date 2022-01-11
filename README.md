# Secure Person2Person Micropayment System
A system with a multithreaded server that stores clients’ user data (usernames, passwords,
and balances), and accomplishes micropayment transactions by allowing one client to send
encrypted messages to another client.

## Features
- Allowed one client to send messages to another client through Linux socket implementation
- Used POSIX Threads library to achieve thread creation, synchronization and termination
- Imported OpenSSL library for basic cryptographic functions to secure client communications

## Compile and Run
You may execute the following commands in the terminal to compile all necessary files:
```
$ make server
$ make client
```

After compiling the program, server and client could be executed in the following ways:
### Server
To execute server program, you will have to set a port number (range: `1024`-`65535`).

- Format:
    ```
    ./server<space><portNumber>
    ```
- Example: Assume that we want to set up a local server and the `portNumber` is `1024`, we have:
    ```
    ./server 1024
    ```
    If the server is successfully set up, the server will create 5 threads and wait for connections. (threadpool implemented)
    

### Client
To execute client program, you will have to set `IP_address` and `portNumber` for the client 
(same as server's `IP_address` and `portNumber`).

- Format:
    ```
    ./client<space><IP_address><space><portNumber>
    ```
- Example:
    Assume that you have already set up a local server and its `portNumber` is `1024`, to make clients connect to the server, we have to execute:
    ```
    ./client 127.0.0.1 1024
    ```
    , where `IP_address` is localhost (`127.0.0.1`) and `portNumber` is `1024`

#### Client Interface
If a client successfully connects to the server, a menu will show up:
```
===[Micropayment System]===
|                         |
|      Input Command:     |
|     'r' -- Register     |
|     'i' -- Login        |
|                         |
===========================
```
Then, a client may call the commands introduced below to navigate:
- `r`: Register an account by providing an username
- `i`: Login by entering your account username and setting a temporary port to get online
- `List`: Display account balance and a list of online users
- `Exit`: Close socket and exit the running program
- `<payer_username>#<payment>#<payee_username>`: Send money to a payee, the payee will receive a message:
    ```
    Incoming payment transaction:
    <payer_username>#<payment>#<payee_username>
    ```
    Once the message is forwarded to the server to handle the transaction, payee will then see the message:
    ```
    You got <payment> from <payer_username>
    Current account balance: <money>
    ```
    (Payer may call `List` to make sure that the transaction was properly handled)