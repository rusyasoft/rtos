#include <stdio.h>
#include <thread.h>
#include <net/ni.h>
#include <net/packet.h>
#include <net/ether.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/ether.h>
#include <string.h>
#include <timer.h>
#include <util/event.h>
#include <util/list.h>
#include <unistd.h>
#include <readline.h>

#define address 0xc0a8640a
#define BUF_SIZE 1460
#define SERVER_PORT 9003

extern uint32_t count2;
uint64_t total_rcv;
uint64_t total_err;
uint32_t err[6];
int socket;
bool flag;
uint8_t buffer[BUF_SIZE +1];

bool bps_checker(void* context) {
	//printf("%u bps\n", total_rcv * 8);
	printf("%u bps, %u, %u, %u, %u, %u, %u, %u\n", total_rcv * 8, *debug_max, *debug_cur,  err[1], err[2], err[3], err[4], err[5]);
	err[2] = 0;
	err[3] = 0;
	err[5] = 0;
	
	total_rcv = 0;

	return true;
}

bool sender(void* context) {
	if(flag) {
		tcp_send(socket, buffer, BUF_SIZE);

//		printf("send %d\n", ret);

	//	if(ret == -1)
	//		return false;
	}
	return true;
}

int my_connected(uint64_t socket, uint32_t addr, uint16_t port, void* context) {
	printf("connected\n");
	flag = true;

	return 1;
}

int my_received(uint64_t socket, const void* buf, size_t len, void* context) {
	total_rcv += len;

	return 1;
}

int my_sent(uint64_t socket, const void* buf, size_t len, void* context) {
	//printf("sent %s\n", buf);
	
	return 1;
}

void destroy() {
}
void gdestroy() {
}

void ginit(int argc, char** argv) {
	NetworkInterface* ni = ni_get(0);
	if(ni != NULL) {
		ni_ip_add(ni, address);
	}

	memset(buffer, 0xff, BUF_SIZE);
	
	event_init();
	total_rcv = 0;
	total_err = 0;
	for(int i = 0; i < 6; i++) {
		err[i] = 0;
	}
	tcp_init();
	event_timer_add(bps_checker, NULL, 0, 1000000);
	
	uint32_t server_ip = 0xc0a86403;
	uint16_t server_port = SERVER_PORT;
	
	flag = false;
	TCPCallback* callback = (TCPCallback*)malloc(sizeof(TCPCallback)); //tcp_callback_create();
	callback->connected = (connected)my_connected;
	callback->sent = (sent)my_sent;
	callback->received = (received)my_received;
	printf("before conn\n");
	socket = tcp_connect(ni, server_ip, server_port, callback, NULL);
	printf("after conn\n");
}
		
void init(int argc, char** argv) {

}


/**
void client_init(){
	uint32_t server_ip = 0xc0a864c8;
	uint16_t server_port = 9001;
	
	TCPCallback* callback = (TCPCallback*)malloc(sizeof(TCPCallback)); //tcp_callback_create();
	callback->connected = (connected)my_connected;
	callback->sent = (sent)my_sent;
	callback->received = (received)my_received;
	tcp_connect(server_ip, server_port, callback, NULL);
	printf("after_conn \n");
}
**/

void process(NetworkInterface* ni){
	Packet* packet = ni_input(ni);
	if(!packet)
		return;

	Ether* ether = (Ether*)(packet->buffer + packet->start);

	if(endian16(ether->type) == ETHER_TYPE_ARP) {
		if(arp_process(packet))
			return;
	} else if(endian16(ether->type) == ETHER_TYPE_IPv4) {
		IP* ip = (IP*)ether->payload;

		if(ip->protocol == IP_PROTOCOL_ICMP && endian32(ip->destination) == address) {
		} else if(ip->protocol == IP_PROTOCOL_UDP) {
		
		} else if(ip->protocol == IP_PROTOCOL_TCP) {
			if(tcp_process(packet))
				return;
		}
	}
	
	if(packet)
		ni_free(packet);
}

int main(int argc, char** argv) {
	printf("Thread %d booting\n", thread_id());

	if(thread_id() == 0) {
		ginit(argc, argv);
	}
	
	thread_barrior();
	
	init(argc, argv);
	
	thread_barrior();
	
//	uint8_t snd_count = 0;
//	uint8_t count = 0;
	NetworkInterface* ni = ni_get(0);

	while(1) {
		if(ni_has_input(ni)) {
			process(ni);
		}
			
		/*
		if(flag && count == 0) {
			int ret;
			if((ret = tcp_send(socket, buffer, BUF_SIZE)) < 0) {
				//printf("ret %d\n", ret);
				ret = -ret;
				err[ret]++;
			}
		}
		count++;
		count = count % 2;
		*/
		
		if(flag/* && snd_count < 21*/) {
			int ret;
			if((ret = tcp_send(socket, buffer, BUF_SIZE)) < 0) {
				//printf("ret %d\n", ret);
				ret = -ret;
				err[ret]++;
			}
			//snd_count++;
		}
		
		event_loop();
	}
	
	thread_barrior();
	
	destroy();
	
	thread_barrior();
	
	if(thread_id() == 0) {
		gdestroy(argc, argv);
	}
	
	return 0;
}

