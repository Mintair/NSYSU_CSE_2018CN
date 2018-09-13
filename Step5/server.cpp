#include "setting.h"

pkt snd_pkt,rcv_pkt[buffer_size/MSS];//send to client,recv from client
int server_fd,seq_num,congestion,fast_retransmit,loss_data,ack_pkt;
int rwnd = buffer_size,ssthresh = threshold,dup_ack,lost_seq,rcv_pkt_num,rcv_pkt_cur_num,loss_num;
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
	cout<<"The threshold = "<<ssthresh<<" bytes"<<endl;
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
	for(int i=0;i<buffer_size/MSS;i++)
		memset(rcv_pkt[i].data , 0 , MSS);
	memset(snd_pkt.data , 0 , MSS);
	srand(time(NULL));
	snd_pkt.src_port = htons(server_addr.sin_port);
	snd_pkt.des_port = htons(client_addr.sin_port);
	snd_pkt.offset_reserved_flag = 0;
	snd_pkt.window_size = 1;
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
		if(recvfrom(server_fd , &rcv_pkt[0] , pkt_size , 0 , (struct sockaddr*)&client_addr , &socklen)!=-1)
			send_msg(socklen,client_addr);
	}
}

int send_msg(socklen_t socklen,struct sockaddr_in client_addr)
{
	//Declaration
	int cwnd = 1,next_data = 1;
	int snd_pkt_num,snd_bytes,file_size;
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
	//First state -> slow start
	cout<<"*****Slow start*****"<<endl;
	//Init snd_pkt
	pkt_init();
	//When file is not sending finsih -> loop
	while(file_size)
	{
		cout<<"cwnd = "<<cwnd<<", rwnd = "<<rwnd<<", threshold = "<<ssthresh<<endl;
		//Calculate the segment
		snd_pkt_num = 1;
		if(cwnd/MSS >= 2)
			snd_pkt_num = cwnd/MSS;
		rcv_pkt_num = snd_pkt_num;
		//Sending segment, if file finished or segments are all sended then stop
		while(snd_pkt_num && file_size)
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
			//Send the packet to client (except 4096 byte)
			if(next_data == 4096 && dup_ack == 0)
			{
				cout<<"\tLoss a packet at : "<<next_data<<" byte, "<<"Data size : "<<snd_bytes<<" bytes"<<endl;
				next_data += snd_bytes;
				rcv_pkt_num--;
			}
			else
			{
				sendto(server_fd , &snd_pkt , header_size + snd_pkt.window_size , 0 , (struct sockaddr*)&client_addr , socklen);	
				cout<<"\tSend a packet at : "<<next_data<<" byte, "<<"Data size : "<<snd_bytes<<" bytes"<<endl;
				next_data += snd_bytes;
				//Receive packet from client
				memset(rcv_pkt[snd_pkt_num-1].data , 0 , MSS);
				rcv_pkt_cur_num = rcv_pkt_num - snd_pkt_num;
				if(recvfrom(server_fd , &rcv_pkt[rcv_pkt_cur_num] , pkt_size , 0 , (struct sockaddr*)&client_addr , &socklen)!=-1)
				{
					//If ack_num == previous ack_num -> dup_ack ++
					if(next_data != rcv_pkt[rcv_pkt_cur_num].ack_num)//rcv_ack -> ERROR!!!!!!!!!!!!!!!
						dup_ack ++;
					else
					{
						loss_num = buffer_size/MSS;
						dup_ack = 0;
					}
					//If dup_ack == 3 -> Fast retransmit
					if(dup_ack == 3)
					{
						loss_num = rcv_pkt_cur_num;
						loss_data = rcv_pkt[rcv_pkt_cur_num].ack_num;
						fast_retransmit = 1;
						break;
					}
				}
			}
			seq_num++;
			snd_pkt_num--;
		}
		//Print the receive packet
		for(int i=0;i<=min(rcv_pkt_cur_num,loss_num);i++)
		{
			cout<<"\tReceive a packet (seq_num = "<<rcv_pkt[i].seq_num<<", ack_num = "<<rcv_pkt[i].ack_num<<")"<<endl;
		}
		//Set the rwnd and cwnd (Congestion avoidance or Slow start)
		rwnd = buffer_size - cwnd;
		//Fast retransmit
		if(fast_retransmit)
		{
			cout<<"*****Fast retransmit*****"<<endl;
			cout<<"*****Slow start*****"<<endl;
			ssthresh = cwnd/2;
			cwnd = 1;
			rwnd = buffer_size;
			file_size += (next_data - loss_data);
			fseek(file , loss_data - next_data , SEEK_CUR);
			seq_num = rcv_pkt[loss_num].seq_num + 1;
			next_data = loss_data;
			fast_retransmit = 0;
		}
		//Congestion avoidance
		else if(congestion)
			cwnd += MSS;
		//Slow start
		else
		{
			cwnd*=2;
			if(cwnd == ssthresh)
			{
				congestion = 1;
				cout<<"*****Congestion avoidance*****"<<endl;
			}
		}
		if(cwnd > buffer_size)
			cwnd = buffer_size;
	}
	//Finish
	congestion = 0;
	//seq_num++;
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

