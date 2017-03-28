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
#define BUF_SIZE 100
#define SERVER_PORT 10000
#define SOCK_NUM 1100
#define SERVER_IP	0xc0a86403

extern uint32_t count2;
uint64_t total_rcv;
uint64_t sockets[SOCK_NUM];
uint32_t conn;
uint32_t send_count;
uint8_t buffer[BUF_SIZE +1];
uint64_t old_time;
NetworkInterface* ni;

int32_t my_connected(uint64_t socket, uint32_t addr, uint16_t port, void* context) {
	uint64_t cur_time = timer_ms();
	conn++;

	if(cur_time - old_time > 500) {
		printf("%u connected!\n", conn);
		old_time = cur_time;
	} 

	send_count = 0;

	if(tcp_send(sockets[send_count], "hello", 5) <= 0) {
		printf("send error!\n");
		while(1);
	}

	return 0;
}

int32_t my_sent(uint64_t socket, size_t len, void* context) {

	return 0;
}

int32_t my_received(uint64_t socket, void* buf, size_t len, void* context) {
	send_count++;

	if(send_count < conn) {
		if(tcp_send(sockets[send_count], "hello", 5) <= 0) {
			printf("send error!\n");
			while(1);
		}
	} else {
		uint64_t tmp_socket = tcp_connect(ni, SERVER_IP, SERVER_PORT);
		if(tmp_socket == 0) {
			printf("%u conn error!\n", conn);
			while(1);
		}
		sockets[conn] = tmp_socket;
		tcp_connected(tmp_socket, my_connected);
		tcp_sent(tmp_socket, my_sent);
		tcp_received(tmp_socket, my_received);
	}

	return 0;
}

void destroy() {
}
void gdestroy() {
}

void ginit(int argc, char** argv) {
	ni = ni_get(0);
	if(ni != NULL) {
		ni_ip_add(ni, address);
	}

	memset(buffer, 0xff, BUF_SIZE);

	conn = 0;
	
	tcp_init();

	uint64_t tmp_socket = tcp_connect(ni, SERVER_IP, SERVER_PORT);
	if(tmp_socket == 0) {
		printf("conn error!\n");
		while(1);
	}

	old_time = timer_ms();

	sockets[conn] = tmp_socket;
	tcp_connected(tmp_socket, my_connected);
	tcp_sent(tmp_socket, my_sent);
	tcp_received(tmp_socket, my_received);
}
		
void init(int argc, char** argv) {

}

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
	
	NetworkInterface* ni = ni_get(0);
	while(1) {
		if(ni_has_input(ni)) {
			process(ni);
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

