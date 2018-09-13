#include "setting.h"

pkt snd_pkt,rcv_pkt;//send to server,recv from server
int client_fd,server_port;
string server_ip;
struct sockaddr_in client_addr,server_addr;

void Input_server();
int create_socket(string server_ip,int server_port);
void pkt_init();
int rcv_msg();

int main(int argc,char* argv[])
{
	int client_port = atoi(argv[1]);
	string client_ip = "127.0.0.1";
	cout<<"=====Parameter====="<<endl;
	cout<<"The RTT delay = "<<RTT<<" ms"<<endl;
	cout<<"The threshold = "<<threshold<<" bytes"<<endl;
	cout<<"The MSS = "<<MSS<<" ms"<<endl;
	cout<<"The buffer size = "<<buffer_size<<" bytes"<<endl;
	cout<<"client's IP is "<<client_ip<<endl;
	cout<<"client is listening on port "<<client_port<<endl;
	Input_server();
	create_socket(client_ip,client_port);
	rcv_msg();
	close(client_fd);
	return 0;
}

int create_socket(string client_ip,int client_port)
{
	client_fd = socket(AF_INET , SOCK_DGRAM , 0);
	if(client_fd==-1)
	{
		perror("Create socket failed.\n");
		return 0;
	}
	
	memset(&client_addr,0,sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = inet_addr(client_ip.c_str());//convert "127.0.0.1" to client address.
	client_addr.sin_port = htons(client_port);
	
	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());//convert "127.0.0.1" to client address.
	server_addr.sin_port = htons(server_port);
	
	if(bind(client_fd,(struct sockaddr *)&client_addr,sizeof(client_addr)) == -1)
	{
		perror("Bind failed.\n");
		return 0;
	}
	return 1;
}

void Input_server()
{
	cout<<"Please input [IP] [PORT] you want to connect:"<<endl;
	cin>>server_ip>>server_port;
}

void pkt_init()
{
	memset(rcv_pkt.data , 0 , MSS);
	memset(snd_pkt.data , 0 , MSS);
	snd_pkt.src_port = htons(client_addr.sin_port);
	snd_pkt.des_port = htons(server_addr.sin_port);
	snd_pkt.window_size = 0;
	snd_pkt.seq_num = 0;
	snd_pkt.ack_num = 1;
}

int rcv_msg()
{
	//Open file to write in
	FILE* file = fopen("Rcv_test.mp4","wb");
	if(file==NULL)
		cout<<"FILE OPEN FAILED."<<endl;
	//Packet initialize
	pkt_init();
	socklen_t socklen;
	socklen = sizeof(server_addr);
	//Establish the connection
	sendto(client_fd , &snd_pkt , header_size + snd_pkt.window_size , 0 , (struct sockaddr*)&server_addr , socklen);
	//Connect
	cout<<"Receive a file from "<<inet_ntoa(server_addr.sin_addr)<<" : "<<htons(server_addr.sin_port)<<endl;
	//Waiting for the message from server
	while(recvfrom(client_fd , &rcv_pkt , pkt_size , 0 , (struct sockaddr*)&server_addr , &socklen)!=-1)
	{
		//Reset sending buffer
		memset(snd_pkt.data , 0 , MSS);
		//Receive in right order
		if(rcv_pkt.offset_reserved_flag == 0)
		{
			//Write in the file
			fwrite(rcv_pkt.data , 1 , rcv_pkt.window_size,file);
			//Set ack_num and seq_num
			snd_pkt.ack_num = rcv_pkt.seq_num + rcv_pkt.window_size;
			snd_pkt.seq_num = rcv_pkt.ack_num;
			cout<<"\tReceive a packet (seq_num = "<<rcv_pkt.seq_num<<", ack_num = "<<rcv_pkt.ack_num<<")"<<endl;
			//Reset receiving buffer
			memset(rcv_pkt.data, 0 , MSS);
		}
		//Sending request to server
		sendto(client_fd , &snd_pkt , header_size + snd_pkt.window_size , 0 , (struct sockaddr*)&server_addr , socklen);
		//File received
		if(rcv_pkt.offset_reserved_flag)
		{
			cout<<"*****File received*****"<<endl;
			break;
		}
	}
	fclose(file);
}

