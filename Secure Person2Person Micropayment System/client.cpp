#include <stdio.h>
#include <stdlib.h>
#include <string.h> //strlen
#include <sys/socket.h>
#include <unistd.h> //close
#include <arpa/inet.h> //inet_addr
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <resolv.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <pthread.h> //for threading, link with lpthread
using namespace std;
void *contact(void*);
void *listening_handler(void *socket_desc);

SSL_CTX* InitCTX(void)
{
	const SSL_METHOD *method;
	SSL_CTX *ctx;

	OpenSSL_add_all_algorithms(); //load cryptos
	SSL_load_error_strings(); //bring in and register error messages
	method = TLSv1_2_client_method(); //create new client-method instance
	ctx = SSL_CTX_new(method); //create new context
	
	if(ctx == NULL)
	{
		ERR_print_errors_fp(stderr);
		abort();
	}
	return ctx;
}

SSL_CTX* InitServerCTX(void)
{
        const SSL_METHOD *method;
        SSL_CTX *ctx;

        OpenSSL_add_all_algorithms(); //load and register all crytos
        SSL_load_error_strings(); //load all error messages
        method = TLSv1_2_server_method(); //create new server-method instance
        ctx = SSL_CTX_new(method); //create new context from method

        if (ctx == NULL)
        {
                ERR_print_errors_fp(stderr);
                abort();
        }
        return ctx;
}

void LoadCertificates(SSL_CTX* ctx, char* CertFile, char* KeyFile)
{
        //set the local certificate from CertFile
        if(SSL_CTX_use_certificate_file(ctx, CertFile, SSL_FILETYPE_PEM) <= 0)
        {
                ERR_print_errors_fp(stderr);
                abort();
        }

        //set the private key from keyfile (maybe same as CertFile)
        if(SSL_CTX_use_PrivateKey_file(ctx, KeyFile, SSL_FILETYPE_PEM) <= 0)
        {
                ERR_print_errors_fp(stderr);
                abort();
        }

        //verify private key
        if(!SSL_CTX_check_private_key(ctx))
        {
                fprintf(stderr, "Private key does not match the public certificate\n");
                abort();
        }
}

//Optional (after ssl handshake)
void ShowCerts(SSL* ssl)
{
	X509 *cert;
	char *line;

	cert = SSL_get_peer_certificate(ssl); //get the server's certificate
	if(cert != NULL)
	{
		cout<<"Server certificates:"<<endl;
		line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
		cout<<"Subject: "<<line<<endl;
		free(line);
		line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
		cout<<"Issuer: "<<line<<endl;
		free(line);
		X509_free(cert); //free the malloc'ed certificate copy
	}
	else
	{
		cout<<"No client certificates configured."<<endl;
	}
}

int getNumFromStr(char* c)
{
	string s = c;
	stringstream str_strm;
	str_strm<<s; //convert the string s into stringstream
	string temp_str;
	int temp_int;
	while(!str_strm.eof()){
		str_strm>>temp_str; //take words into temp_str one by one
		if(stringstream(temp_str)>>temp_int)
		{
			return temp_int;
		}
		temp_str = ""; //clear temp string
	}
	return -1;
}

inline bool isInteger(const string & s)
{
	if(s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) 
		return false;
	char * p;
	strtol(s.c_str(), &p, 10);

	return (*p == 0);
}

struct user_data
{
	string username; //store account username
	string ip_addr; //store account IP address
	string port; //store account port number
};

SSL *main_ssl;
string account_name, payee_name, payee_ip, amount;
int account_money, extra_port, payee_port;
struct user_data data[100]; //store users online data

void get_data(char* in)
{
	size_t pos, space;
	int n;
	string msg = in;
	string flag[3] = {"Username:", "IP address:", "Port:"};
	
	//Get account balance
	pos = msg.find("balance: ");
	pos+=9;
	space = msg.find("\n",pos);
	account_money = stoi(msg.substr(pos,space-pos));

	for(int i = 0; i < 3; i++)
	{
		pos = 0;
		space = 0;
		n = 0;
		while((pos = msg.find(flag[i], pos)) != string::npos)
		{
			if(i==0)
			{
				pos += 9;
				space = msg.find(" /", pos);
				data[n].username = msg.substr(pos, space-pos);
			}
			if(i==1)
			{
				pos += 11;
				space = msg.find(" /", pos);
				data[n].ip_addr = msg.substr(pos, space-pos);
			}
			if(i==2)
			{
				pos += 5;
				space = msg.find("\n", pos);
				data[n].port = msg.substr(pos, space-pos);
			}
			n++;
			pos++;
		}
	}
}

int main(int argc, char *argv[])
{
	char* IpAddr = argv[1];
	int port = atoi(argv[2]), temp;
	int socket_desc, in_msg, code, *sock;
	int client_state = -1, dc = 0, callList = 0, needthread = 0;
	struct sockaddr_in server;
	char *message, server_reply[256];
	string msg, username, portNum;

	SSL_CTX *ctx;
	SSL *ssl;
	
	//SSL initiate
	SSL_library_init();
	ctx = InitCTX();

	
	//Create socket
	socket_desc = socket(PF_INET, SOCK_STREAM, 0);
	if(socket_desc == -1)
	{
		cout<<"**Fail to create socket.**"<<endl;
	}
	else
	{
		cout<<"Socket created."<<endl;
	}
	
	server.sin_addr.s_addr = inet_addr(IpAddr); //IP addr->long format
	server.sin_family = AF_INET; //IPv4
	server.sin_port = htons(port); //Host to network short integer
	
        ssl = SSL_new(ctx); //create new SSL connection state
        SSL_set_fd(ssl, socket_desc); //attach the socket descriptor

	//Open connection to remote server
	if(connect(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
		cout<<"**Connection error.**"<<endl;
		return 1;
	}
	if(SSL_connect(ssl) < 0)
	{
		cout<<"**Fail to perform SSL connection.**"<<endl;
		return 1;
	}
	main_ssl = ssl;
	in_msg = SSL_read(ssl, server_reply, sizeof(server_reply));
	server_reply[in_msg] = '\0'; //Add terminator
	cout<<server_reply; //Connection accepted
	cout<<""<<endl;

	//Data transfer
	while(1)
	{
		//Send message by different client state
		switch(client_state){
			case(-1):
        			cout<<"===[Micropayment System]==="<<endl;
        			cout<<"|                         |"<<endl;
        			cout<<"|      Input Command:     |"<<endl;
        			cout<<"|     'r' -- Register     |"<<endl;
        			cout<<"|     'i' -- Login        |"<<endl;
        			cout<<"|                         |"<<endl;
        			cout<<"==========================="<<endl;
        			//Input command to continue
                		cin>>msg;
                		if(msg == "r\0")
                		{
                        		client_state = 0;
                		}
                		else if(msg == "i\0")
                		{
                        		client_state = 1;
                		}
                		else
                		{
                        		cout<<"(*Please input the right command)"<<endl;
                		}
				continue;

			case(0):
				//Enter username to register
				//Format: REGISTER#<username><CRLF>
				cout<<"[Register]"<<endl;
				cout<<"Please enter your Username:"<<endl;
				cin>>(username);
				msg = "REGISTER#" + username;
				message = &msg[0];
				SSL_write(ssl, message, strlen(message));
				client_state = 1;
				break;
		
			case(1):	
				//Login with username and port number
				//Format: <username>#<port><CRLF>
				cout<<"[Login]"<<endl;
				cout<<"Username:"<<endl;
        		       	cin>>(username);
				cout<<"port number:"<<endl; 
			      	cin>>portNum;
               		  	msg = username + "#" + portNum;
               		 	message = &msg[0];

				if(isInteger(portNum))
				{
					temp = stoi(portNum);
					if((temp != port) && (temp>=1024 && temp <= 65535))
					{
						callList = 1; //get data
						needthread = 1; //for connection
               	        			SSL_write(ssl, message, strlen(message));
						client_state = 2;
					}
					else
					{
						cout<<"**Port number invalid**"<<endl;
						continue;
					}
				}
				else
				{
					cout<<"**Port number invalid**"<<endl;
					continue;
				}
				break;

			case(2):
				//Call List, Exit or communicate with others
				cin>>msg;
				if(msg == "Exit\0")
				{
					client_state = 3;
				}
				else
				{
					if(msg == "List\0")
					{
						memset(data, 0, sizeof(data)); //clear data
						callList = 1; //get ready to get list
						msg = msg + "\n";
                                        	message = &msg[0];
                                        	SSL_write(ssl, message, strlen(message));
					}
					else
					{
						int n = 0;
						size_t pos = 0, last = 0;
						string myname, payment, payee;
						
						//Extract
						while((pos = msg.find('#',pos)) != string::npos)
						{
							if(n==0)
							{
								myname = msg.substr(0,pos);
								last = pos;
							}
							if(n==1)
							{
								payment = msg.substr(last+1, pos-last-1);
								payee = msg.substr(pos+1);
							}
							n++;
							pos++;
						}

						//Check and connect to client
						if((n==2) && (myname == account_name) && isInteger(payment) && payee != myname)
						{	
							int i = 0;
							cout<<"(Checking client-client message...)"<<endl;
							//check payee online or not
							for(i = 0; i < 100; i++)
							{
								if(data[i].username == payee)
								{
									//connect to payee and send msg
									payee_name = data[i].username;
									payee_ip = data[i].ip_addr;
									payee_port = stoi(data[i].port);
									amount = payment;
                                                                	pthread_t t_id;
                                                                	pthread_create(&t_id, NULL, contact, NULL);
									break;
								}
							}
							if(i==100) {cout<<"**Payee not found**"<<endl;}
						}
						else
						{
							cout<<"**Incorrect format**"<<endl;
						}
						continue;
					}
					break;
				}

			case(3):
				//Client ready to leave
				msg = "Exit\n";
                                message = &msg[0];
                                SSL_write(ssl, message, strlen(message));
				dc = 1;
				break;
				
		}

		in_msg = SSL_read(ssl, server_reply, sizeof(server_reply)); //Receive number of bytes

                if(client_state >= 0)
                {
                        code = getNumFromStr(server_reply); //Get result
                        if(code == 210){
                                //FAIL: cannot use the input username
                                client_state = -1;
                                cout<<"(*You can't use that username)"<<endl;
                        }
                        if(code == 220){
                                //AUTH FAIL: login issue
                                client_state = -1;
				needthread = 0;
				callList = 0;
                                cout<<"(*Login issue)"<<endl;
                        }
                }

                cout<<"------------------------"<<endl;
                cout<<"[Server]:"<<endl;
                //Receive message from server
                if(in_msg == -1)
                {
                        cout<<"**Error on receiving message**"<<endl;
                }
                else
                {
                        //Because server_reply is not null terminated,
                        //result of strlen() is undefined.
                        //Hence, add terminator to receving data bytes.
                        server_reply[in_msg] = '\0'; //add terminator
                        cout<<server_reply;
			if(callList == 1)
                        {
				account_name = username;
				get_data(server_reply);
				callList = 0;
				if(needthread == 1)
				{
					sock = (int*)malloc(sizeof(int));
                                        *sock = socket_desc;
                                        extra_port = temp;
                                        pthread_t t_id;
                                        pthread_create(&t_id, NULL, listening_handler, (void*) sock);
					needthread = 0;
				}
                        }
                }
                cout<<""<<endl;
                cout<<"------------------------"<<endl;

		
		//Close connection
		if(dc == 1 && client_state == 3)
		{
			cout<<"(Closing socket...)"<<endl;
			SSL_free(ssl); //release connection state
               		close(socket_desc); //close socket
			SSL_CTX_free(ctx); //release context
                	cout<<"Disconnected."<<endl;
			break;
		}
	}

	return 0;
}

void *contact(void *nan)
{
	int thread_sock;
        struct sockaddr_in s;
	char *message;
	string msg;
        SSL_CTX *ctx;
	SSL *ssl;

        SSL_library_init();
        ctx = InitCTX(); //initialize SSL

        thread_sock = socket(PF_INET, SOCK_STREAM, 0);

        s.sin_addr.s_addr = inet_addr(payee_ip.c_str());
        s.sin_family = AF_INET;
        s.sin_port = htons(payee_port);
                                
	ssl = SSL_new(ctx); //create new SSL connection state
        SSL_set_fd(ssl, thread_sock); //attach the socket descriptor

        //Open connection to remote server
        if(connect(thread_sock, (struct sockaddr *)&s, sizeof(s)) < 0)
        {
                cout<<"**Connection error.**"<<endl;
                return 0;
        }
        if(SSL_connect(ssl) < 0)
        {
                cout<<"**Fail to perform SSL connection.**"<<endl;
                return 0;
        }

	if(stoi(amount) <= account_money)
	{
		account_money -= stoi(amount);
		msg = account_name + "#" + amount + "#" + payee_name;
		msg = msg + "\n";
		message = &msg[0];
		SSL_write(ssl, message, strlen(message));
		cout<<"(Payment sent to "<<payee_name <<")"<<endl;
	}
	else
	{
		cout<<"**You don't have that money!**"<<endl;
	}
	SSL_free(ssl); //release connection state
        close(thread_sock); //close socket
        SSL_CTX_free(ctx); //release context
	return 0;
}

void *listening_handler(void *socket_desc)
{
	int thread_sock;
	int c, in_msg, new_socket; 
	//int sock = *(int*)socket_desc;
	char client_message[256];
	struct sockaddr_in s, client;

	SSL_CTX *ctx;
        SSL_library_init();
        ctx = InitServerCTX(); //initialize SSL
        LoadCertificates(ctx, (char *)"mycert.pem", (char *)"mycert.pem"); //load certs

	thread_sock = socket(PF_INET, SOCK_STREAM, 0); //another socket for micropayment msg

	//Prepare
	s.sin_addr.s_addr = INADDR_ANY; //IP addr->long format
	s.sin_family = AF_INET; //IPv4
	s.sin_port = htons(extra_port); //Host to network short integer

	//Bind
	if(bind(thread_sock, (struct sockaddr *) &s, sizeof(s)) < 0)
	{
		cout<<"Bind "<<extra_port<<" failed"<<endl;
		return 0;
	}
	cout<<"Bind "<<extra_port<<" succeed"<<endl;
	
	//Listen
	listen(thread_sock, 3);

	//Accept the incoming connection
	c = sizeof(struct sockaddr_in);
	while(1)
	{
		SSL *ssl;
		new_socket = accept(thread_sock, (struct sockaddr *)&client, (socklen_t*)&c);

		if(new_socket<0)
                {
                        cout<<"**Fail to connect.**"<<endl;
                }
                else
                {
                        cout<<"(Someone just connected to you.)"<<endl;
			ssl = SSL_new(ctx); //get new SSL state with context
                        SSL_set_fd(ssl, new_socket); //set connection socket to SSL state
			//Do SSL-protocol accept (SSL handshake)
        		if(SSL_accept(ssl) < 0)
        		{
                		ERR_print_errors_fp(stderr);
                		return 0;
        		}

			cout<<"-----------------------------"<<endl;
			cout<<"Incoming payment transaction:"<<endl;
			in_msg = SSL_read(ssl, client_message, sizeof(client_message));
			client_message[in_msg] = '\0'; //Add terminator
			cout<<client_message;
			cout<<""<<endl;
			cout<<"-----------------------------"<<endl;
			
			//Transfer msg to server
			cout<<"(Transfer message to server)"<<endl;
			SSL_write(main_ssl, client_message, sizeof(client_message));

			//Get result from server
			in_msg = SSL_read(main_ssl, client_message, sizeof(client_message));
                        client_message[in_msg] = '\0'; //Add terminator
                        cout<<client_message;
			cout<<""<<endl;

		}
	}
	return 0;
}







