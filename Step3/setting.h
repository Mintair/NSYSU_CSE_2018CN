#include <iostream>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/errno.h>
#include <string.h>
#include <arpa/inet.h>
using namespace std;
#define RTT 200
#define threshold 65536
#define MSS 1024
#define buffer_size 32768
#define pkt_size sizeof(pkt)
#define header_size pkt_size-MSS
typedef struct pkt
{
	unsigned short src_port,des_port;
	unsigned int ack_num,seq_num;
	unsigned short offset_reserved_flag,window_size;
	unsigned short checksum,urg_pointer;
	unsigned int option;
	char data[MSS];
}pkt;
