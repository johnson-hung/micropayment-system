#include <iostream>
#include <sstream> //stringstream
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //strlen
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h> //write
#include <pthread.h> //for threading, link with lpthread
#include <assert.h>
#include "Thread.h"
#include <resolv.h>
#include "openssl/ssl.h"
#include "openssl/err.h"
using namespace std;

int connection_handler(SSL *ssl);

class threadArg
{
	public:
		int socket; //Client's socket number
		char ip[INET_ADDRSTRLEN]; //ip: IP_address
		string name, port; //name: registered name, port: login port
		int on, used, money; //on: online state, used: 1 if this thread is used
		void clearSome()
		{
			//Get called when client is going to leave
			//Keep: name, money, socket, used
			//Clear: ip, port, on
			memset(ip, 0, sizeof(ip));
			port.clear();
			on = 0;
		}
		void clearAll()
		{
			//Clear all
			socket = 0;
			memset(ip, 0,sizeof(ip));
			name.clear();
			port.clear();
			money = 0;
			on = 0;
			used = 0;
		}
};

threadArg data[100];
SSL *curr_ssl;

class CMyTask: public CTask
{
	public:
		CMyTask() = default;
		SSL *ssl = curr_ssl;
		int Run()
		{
			int connfd = GetConnFd();

			//request and response
			connection_handler(ssl);
			close(connfd);
			return 0;
		}
};

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
	//Load server certificate into the SSL context
	if(SSL_CTX_use_certificate_file(ctx, CertFile, SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		abort();
	}
	
	//Load server private key into the SSL context
	if(SSL_CTX_use_PrivateKey_file(ctx, KeyFile, SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		abort();
	}

	//Verify private key
	if(!SSL_CTX_check_private_key(ctx))
	{
		fprintf(stderr, "Private key does not match the public certificate\n");
		abort();
	}
}


//Optional(after ssl handshake)
void ShowCerts(SSL* ssl)
{
	X509 *cert;
	char *line;

	cert = SSL_get_peer_certificate(ssl); //get certificates(if available)
	if(cert != NULL)
	{
		cout<<"Server certificates:"<<endl;
		line = X509_NAME_oneline(X509_get_subject_name(cert), 0 , 0);
		cout<<"Subject: "<<line<<endl;
		free(line);
		line = X509_NAME_oneline(X509_get_issuer_name(cert), 0 , 0);
		cout<<"Issuers: "<<line<<endl;
		free(line);
		X509_free(cert);
	}
	else
	{
		cout<<"No certificates."<<endl;
	}
}


int main(int argc, char *argv[])
{
	int port = atoi(argv[1]);
	int socket_desc, new_socket;
	struct sockaddr_in server, client;
	
	SSL_CTX *ctx;

	//Check port
	if(port<1024 || port>65535)
	{
		cout<<"Please input valid port number. (1000-65535)"<<endl;
		return 1;
	}

	SSL_library_init();
        ctx = InitServerCTX(); //initialize SSL
        LoadCertificates(ctx, (char *)"mycert.pem", (char *)"mycert.pem"); //load certs
	
	//Create socket
	socket_desc = socket(PF_INET, SOCK_STREAM, 0); 
	if(socket_desc == -1)
	{
		cout<<"Fail to create socket."<<endl;
	}
	
	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET; //IPv4
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);

	//Bind
	if(bind(socket_desc,(struct sockaddr *)&server, sizeof(server)) < 0)
	{
		cout<<"Bind failed."<<endl;
	}
	cout<<"Bind done!"<<endl;
	
	//Listen
	listen(socket_desc, 5);

	//Create threadpool
	CThreadPool Pool(5);
	cout<<"Waiting for incoming connections..."<<endl;

	while(1)
	{
		socklen_t len = sizeof(struct sockaddr_in); //set socket address length
		SSL *ssl;

		new_socket = accept(socket_desc, (struct sockaddr *) &client, &len);
		if(new_socket<0)
		{
			cout<<"Fail to connect."<<endl;
		}
		else
		{
			cout<<"Connection accepted!"<<endl;

			ssl = SSL_new(ctx); //get new SSL state with context
			SSL_set_fd(ssl, new_socket); //set connection socket to SSL state

			//Add task
			curr_ssl = ssl;
			CTask* ta = new CMyTask;
			ta->SetConnFd(new_socket);
			Pool.AddTask(ta);
			
			//find empty space for client, then set data
			int temp = -1;
			for(unsigned int i = 0; i < sizeof(data)/sizeof(data[0]) ; i++)
			{
				if(data[i].used == 0)
				{
					//find empty space for client
					temp = i;
					inet_ntop(AF_INET,&client.sin_addr,data[i].ip,INET_ADDRSTRLEN);
					data[i].socket = new_socket;
					data[i].used = 1;
					break;
				}
			}
			if(temp == -1)
			{
				return 1;
			}
		}
	}
	/*
	if(new_socket < 0)
	{
		cout<<"Accept failed."<<endl;
		return 1;
	}
	*/
	close(socket_desc); //close server socket
	SSL_CTX_free(ctx); //release context
	return 0;
}

//Handle connection for each client
int connection_handler(SSL* ssl)
{
	class threadArg *myData;
	int in_msg;
	char *message, client_message[256];
	string msg;
	int online = 0;
	stringstream ss;
	size_t p = 0; //initial place
	
	//Do SSL-protocol accept (SSL handshake)
	if(SSL_accept(ssl) < 0)
	{
		ERR_print_errors_fp(stderr);
		return 1;
	}
	
	//ShowCerts(ssl); //get any certificates
	
	for(p = 0; p < sizeof(data)/sizeof(data[0]); p++)
	{
		if(data[p].used==0)
		{
			break;
		}
	}
	myData = &data[p-1]; //Get initial data from place (the lastest one)

	//notify
	msg = "Successfully connected!\n";
	message = &msg[0];
	SSL_write(ssl, message, 256);

	//Receive message from client
	while(1)
	{
		in_msg = SSL_read(ssl, client_message, 256);
		client_message[in_msg] = '\0';
		msg = client_message;
		cout<<msg<<endl;
		int check_reg = 0; //for checking registered or not (1: already registered)
		int check_log = 0; //for checking logged in or not (1: already logged or notfound)
		int n = 0;
		
		//when client logs out, we have to set its port and on back to 1
        	if(in_msg == 0)
        	{
                	cout<<"Client disconnected."<<endl;
			myData->clearSome();
			return 0;
        	}
        	else if(in_msg == -1)
        	{
                	//cout<<"Failed to receive."<<endl;
        	}

		for(unsigned int i = 0; i < msg.length(); i++)
		{
			if(msg[i] == '#')
			{
				n++;
			}
		}

		if(n == 0)
		{
			if(msg == "List\n")
			{
				//Get users online
				online = 0;
				for(unsigned int i = 0; i < sizeof(data)/sizeof(data[0]); i++)
        			{
                			if(data[i].on == 1)
                			{
                        			online++;
                			}
        			}

				stringstream ss;
				ss << myData->money;
				msg = "Account balance: " + ss.str();
				ss.str(""); //clear the contents of a stringstream
				ss << online;
				msg += "\nNumber of accounts online: " + ss.str();
				//Client call list
				for(unsigned int j = 0; j < sizeof(data)/sizeof(data[0]); j++)
				{
					if(data[j].on == 1)
					{
						msg += "\n- Username:"+ data[j].name
							+" /IP address:"+ data[j].ip
							+" /Port:"+data[j].port;
					}
				}
				message = &msg[0];
			}
			else if(msg == "Exit\n")
			{
				myData->clearSome();
				msg = "Bye";
				message = &msg[0];
			}
			else
			{
				msg = "Please input the right command.";
				message = &msg[0];
			}
		}
		else if(n == 1)
		{
			//Check it's register or login
			if(msg.substr(0,msg.find("#")) == "REGISTER")
			{
				//Register
				string in_name = msg.substr(msg.find("#")+1);

                       		for(unsigned int i = 0; i < sizeof(data)/sizeof(data[0]); i++)
                       		{
                               		if(data[i].name == in_name)
                               		{
                                       		check_reg = 1;
                                       		break;
					}
	                        }

				if(check_reg == 1)
                       		{
                               		msg = "210 AUTH_FAIL";
                               		message = &msg[0];
                       		}       
                 		else
                        	{
					//Find an empty space
					for(unsigned int i = 0; i < sizeof(data)/sizeof(data[0]); i++)
                                	{
                                        	if(data[i].used == 0)
                                        	{
                                                	data[i].name = in_name;
							data[i].money = 10000;
							data[i].used = 1;
                                                	break;
                                        	}
                                	}
                               		msg = "100 OK";
                               		message = &msg[0];
                               		cout<<in_name<<" just registered."<<endl;
                       		}
			}
			else
			{
				//Login
				string in_name = msg.substr(0,msg.find("#"));
				string in_port = msg.substr(msg.find("#")+1);
				for(unsigned int i = 0; i < sizeof(data)/sizeof(data[0]); i++)
				{
					if(data[i].name == in_name)
					{
						if(data[i].on == 0)
						{
							//nobody is using, client can log in
							data[i].port = in_port;
							data[i].socket = myData->socket;
							strcpy(data[i].ip, myData->ip);
							data[i].on = 1;
							myData->clearSome();//clear current place
							myData = &data[i];  //move to another place
							check_log = 0;
						}
						else
						{
							//already logged in
							check_log = 1;
						}
						break;
					}
					else
					{
						check_log = 1; //username not found
					}
				}

				if(check_log == 1)
                                {
                                        msg = "220 AUTH_FAIL";
                                        message = &msg[0];
                                }
                                else
                                {
					//Get users online, then send list
                                	online = 0;
                                	for(unsigned int i = 0; i < sizeof(data)/sizeof(data[0]); i++)
                                	{
                                        	if(data[i].on == 1)
                                        	{
                                               		online++;
                                        	}
                                	}
                                	stringstream ss;
                                	ss << myData->money;
                                	msg = "Account balance: " + ss.str();
                                	ss.str(""); //clear the contents of a stringstream
                                	ss << online;
                                	msg += "\nNumber of accounts online: " + ss.str();
                                	//Send current list
                                	for(unsigned int j = 0; j < sizeof(data)/sizeof(data[0]); j++)
                                	{
                                        	if(data[j].on == 1)
                                        	{
                                                	msg += "\n- Username:"+ data[j].name
                                                        	+" /IP address:"+ data[j].ip
                                                        	+" /Port:"+data[j].port;
                                        	}
                                	}
                                	message = &msg[0];
                                        cout<< myData->name <<" just logged in./ port: "
						<< myData->port <<endl;
                                }
			}
		}
		else if(n == 2)
		{
			//Payment transaction
			int temp = 0;
			size_t pos = 0, last = 0;
			string payer, payee, payment;
			//Extract
			while((pos = msg.find('#',pos)) != string::npos)
                        {
				if(temp==0)
				{
					payer = msg.substr(0,pos);
					last = pos;
				}
				if(temp==1)
				{
					payment = msg.substr(last+1, pos-last-1);
					payee = msg.substr(pos+1);
				}
 				temp++;
				pos++;
			}
			for(unsigned int i = 0; i < sizeof(data)/sizeof(data[0]);i++)
			{
				if(data[i].on == 0){continue;}

				if(data[i].name == payer)
				{
					data[i].money -= stoi(payment);
					msg = "You got "+ payment + " from " + data[i].name;
				}
			}
			myData->money += stoi(payment);
			stringstream ss;
			ss << myData->money;
			msg += "\nCurrent account balance: "+ ss.str();
			message = &msg[0];
			ss.str("");
		}
		else
		{
			msg = "Please input the right command.";
                        message = &msg[0];
		}

		//Send final response
		SSL_write(ssl, message, strlen(message));
	}
	SSL_free(ssl);
	return 0;
}


















