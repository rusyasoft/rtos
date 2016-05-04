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

void destroy() {
}
void gdestroy() {
}

void ginit(int argc, char** argv) {
	NetworkInterface* ni = ni_get(0);
	if(ni != NULL) {
		ni_ip_add(ni, address);
	}
}
		
void init(int argc, char** argv) {

}

//void *arg
static int my_connected(uint64_t socket, uint32_t addr, uint16_t port, void* context) {
	char str[100];
	printf("connected\n");
	strcpy(str, "HELO@@@$$$4");
	tcp_send(socket, str, sizeof(str));
	return 1;
}

static int my_received(uint64_t socket, const void* buf, size_t len, void* context) {
	//printf("my receive %s\n", buf);
	tcp_send(socket, buf, len);
	return 1;
}

static int my_sent(uint64_t socket, const void* buf, size_t len, void* context) {
	//printf("sent %s\n", buf);
	
	return 1;
}

void client_init(){
	//printf("client_init start \n");
	uint32_t server_ip = 0xc0a864c8;
	uint16_t server_port = 9001;

	TCPCallback* callback = (TCPCallback*)malloc(sizeof(TCPCallback)); //tcp_callback_create();
	callback->connected = (connected)my_connected;
	callback->sent = (sent)my_sent;
	callback->received = (received)my_received;
	tcp_connect(server_ip, server_port, callback, NULL);
	printf("hshshshsh \n");
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
			tcp_process(ip);
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
	event_init();
	while(1) {
		if(ni_has_input(ni)) {
			process(ni);
		}

		char* line = readline();
		if(line) {
			if(!strcmp(line, "connect")) {
				client_init();
			} 
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

