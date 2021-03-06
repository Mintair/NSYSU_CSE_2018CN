#include "setting.h"

pkt snd_pkt,rcv_pkt;//send to client,recv from client
int server_fd,seq_num,total_delay_ack;
struct sockaddr_in server_addr,client_addr;//record server/client's address

int create_socket(string server_ip,int server_port);
void pkt_init();
void listen();
int send_msg(socklen_t socklen,struct sockaddr_in client_addr);

int main(int argc,char* argv[])
{
	int server_port = atoi(argv[1]);
	string server_ip = "127.0.0.1";
	cout<<"=====Parameter====="<<endl;
	cout<<"The RTT delay = "<<RTT<<" ms"<<endl;
	cout<<"The threshold = "<<threshold<<" bytes"<<endl;
	cout<<"The MSS = "<<MSS<<" ms"<<endl;
	cout<<"The buffer size = "<<buffer_size<<" bytes"<<endl;
	cout<<"server's IP is "<<server_ip<<endl;
	cout<<"server is listening on port "<<server_port<<endl;
	create_socket(server_ip,server_port);
	listen();
	close(server_fd);
	return 0;
}

int create_socket(string server_ip,int server_port)
{
	server_fd = socket(AF_INET , SOCK_DGRAM , 0);
	if(server_fd==-1)
	{
		perror("Create socket failed.\n");
		return 0;
	}
	
	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());//convert "127.0.0.1" to server address.
	server_addr.sin_port = htons(server_port);
	
	if(bind(server_fd,(struct sockaddr *)&server_addr,sizeof(server_addr)) == -1)
	{
		perror("Bind failed.\n");
		return 0;
	}
	return 1;
}

void pkt_init()
{
	memset(rcv_pkt.data , 0 , MSS);
	memset(snd_pkt.data , 0 , MSS);
	srand(time(NULL));
	snd_pkt.src_port = htons(server_addr.sin_port);
	snd_pkt.des_port = htons(client_addr.sin_port);
	snd_pkt.offset_reserved_flag = 0;
	snd_pkt.window_size = 0;
	snd_pkt.seq_num = 1;
	snd_pkt.ack_num = rand()%10000+1;
	seq_num = snd_pkt.ack_num;
}

void listen()
{
	socklen_t socklen;
	socklen = sizeof(client_addr);
	while(1)
	{
		//cout<<"Wait for client..."<<endl;
		if(recvfrom(server_fd , &rcv_pkt , pkt_size , 0 , (struct sockaddr*)&client_addr , &socklen)!=-1)
			send_msg(socklen,client_addr);
	}
}

int send_msg(socklen_t socklen,struct sockaddr_in client_addr)
{
	//Declaration
	int cwnd = 1,next_data = 1,seg_c,rwnd = buffer_size,file_size,snd_bytes,delay_ack;
	//Open the file
	FILE* file = fopen("Video1.mp4","rb");
	if(file==NULL)
		cout<<"FILE OPEN FAILED."<<endl;
	//Get the file size
	fseek(file , 0 , SEEK_END);
	file_size = ftell(file);
	fseek(file , 0 , SEEK_SET);
	//Start sending file
	cout<<"Start to send file to "<<inet_ntoa(client_addr.sin_addr)<<" : "<<htons(client_addr.sin_port)<<", the file size is "<<file_size<<" bytes"<<endl;
	//Init snd_pkt
	pkt_init();
	//When file is not sending finsih -> loop
	while(file_size)
	{
		cout<<"cwnd = "<<cwnd<<", rwnd = "<<rwnd<<", threshold = "<<threshold<<endl;
		//Calculate the segment
		seg_c = 1;
		if(cwnd/MSS >= 2)
			seg_c = cwnd/MSS;
		//Sending segment, if file finished or segments are all sended then stop
		while(seg_c && file_size)
		{
			//Calculate the sending bytes
			snd_bytes = min(cwnd,MSS);
			snd_bytes = min(snd_bytes,file_size);
			//Reset buffer,then read the file into the buffer
			memset(snd_pkt.data , 0 , MSS);
			fread(snd_pkt.data , 1 , snd_bytes , file);
			file_size -= snd_bytes;
			snd_pkt.window_size = snd_bytes;
			//Set packet's seq_num and ack_num
			snd_pkt.seq_num = next_data;
			snd_pkt.ack_num = seq_num;
			//Send the packet to client
			sendto(server_fd , &snd_pkt , header_size + snd_pkt.window_size , 0 , (struct sockaddr*)&client_addr , socklen);
			cout<<"\tSend a packet at : "<<next_data<<" byte, "<<"Data size : "<<snd_bytes<<" bytes"<<endl;
			next_data += snd_bytes;
			seq_num++;
			//Calculate the packet which is unacked
			delay_ack++;
			total_delay_ack = delay_ack;
			
			seg_c--;
		}
		//Wait until client ack (if delay 2 packet or last packet)
		while(delay_ack / 2 || file_size == 0)
		{
			if(delay_ack <= 0)
				break;
			//Reset receive buffer
			memset(rcv_pkt.data , 0 , MSS);
			//Receive packet from client
			if(recvfrom(server_fd , &rcv_pkt , pkt_size , 0 , (struct sockaddr*)&client_addr , &socklen)!=-1)
			{
				//Record for next sending data's index
				next_data = rcv_pkt.ack_num;
				cout<<"\tReceive a packet (seq_num = "<<rcv_pkt.seq_num<<", ack_num = "<<rcv_pkt.ack_num<<")"<<endl;
				//One ack two packet
				delay_ack -= 2;
				//Delay ack cross two sending
				if(delay_ack == 1)
					next_data += snd_bytes;
			}
		}
		
		//Set the rwnd and cwnd
		rwnd = buffer_size - cwnd;
		cwnd*=2;
		if(cwnd > buffer_size)
			cwnd = buffer_size;
	}
	//Finish
	total_delay_ack = 0;
	snd_bytes = 0;
	//Set finish flag
	snd_pkt.offset_reserved_flag = 1;
	snd_pkt.seq_num = next_data;
	snd_pkt.ack_num = seq_num;
	//Send the last packet to inform client transmission is finish
	sendto(server_fd , &snd_pkt , header_size + snd_pkt.window_size , 0 , (struct sockaddr*)&client_addr , socklen);
	recvfrom(server_fd , &rcv_pkt , pkt_size , 0 , (struct sockaddr*)&client_addr , &socklen);
	cout<<"*****File sended*****"<<endl;
	//Close the file
	fclose(file);
}

