#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <util/event.h>

#include <net/packet.h>
#include <net/ni.h>
#include <net/interface.h>
#include <net/arp.h>
#include <net/ether.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/checksum.h>

#define MAX_SQUENCENUM 	2147483648
#define ADDRESS 0xc0a8640a

#define TCP_CLOSED		0	
#define TCP_LISTEN 		1
#define TCP_SYN_RVCD		2
#define TCP_SYN_SENT		3
#define TCP_ESTABLISHED		4
#define TCP_CLOSE_WAIT		5
#define TCP_LAST_ACK		6
#define TCP_FIN_WAIT_1		7
#define TCP_FIN_WAIT_2		8
#define TCP_CLOSING		9
#define TCP_TIME_WAIT		10
#define TCP_TCB_CREATED		11

#define SND_WND_MAX 43690
#define RECV_WND_MAX 43690

typedef struct {
	uint32_t sip;
	uint16_t sport;
	uint32_t swin;

	uint32_t dip;
	uint32_t dport;
	uint32_t dwin;

	int state;
	uint32_t seq;
	uint32_t ackno;
	uint64_t timer_id;
	TCPCallback* callback;
	void* context;

	List* seg_sent;	// sent segment but no ack.
	uint32_t snd_wnd_max;
	uint32_t snd_wnd_cur;
	uint32_t recv_wnd_max;
	uint32_t recv_wnd_cur;
} TCB;

static uint32_t ip_id;
static int arp_state;
static Map* tcbs = NULL;

static bool packet_out(TCB* tcb, int syn, int ack, int psh, int fin, const void* str);

static uint32_t tcp_init_seqnum() {
	uint64_t time;
	uint32_t* p = (uint32_t*)&time;
	asm volatile("rdtsc" : "=a"(p[0]), "=d"(p[1]));
	return time % MAX_SQUENCENUM;
}

static uint32_t tcp_get_seqnum(int ack, TCB* tcb) {
	if(ack == 1) {
		return (tcb->seq);
	} else {
		if((tcb->seq) == MAX_SQUENCENUM) {
			(tcb->seq) = 0;
		} else {
			(tcb->seq)++;
		}
	}
	return	(tcb->seq);
}

static void ip_init_id() {
	//TODO: needs something different
	ip_id = 0x8000;
}

static uint32_t ip_get_id(int ack) {
	if(ack == 1) {
		return ip_id;
	} else {
		ip_id++;
	}
	return	ip_id;
}

// TODO: Need something other than this
static uint32_t tcb_key_create(uint32_t sip, uint16_t sport) {
	uint32_t tcb_key = (sip & 0xffff);
	tcb_key |= (sport & 0xff) ;
	return tcb_key;
}

static TCB* tcb_create(NetworkInterface* ni, uint32_t addr, uint16_t port, TCPCallback* callback) {
	TCB* tcb = (TCB*)malloc(sizeof(TCB));
	if(tcb == NULL)
		printf("tcb NULL\n");

	tcb->sip = ADDRESS; 
	tcb->sport = tcp_port_alloc(ni, ADDRESS);
	tcb->dip = addr;
	tcb->dport = port;
	
	tcb->state = TCP_CLOSED;
	tcb->seq = tcp_init_seqnum();
	tcb->ackno = 0; //TODO : ackno
	tcb->callback = callback;

	tcb->recv_wnd_max = RECV_WND_MAX;
	tcb->snd_wnd_max = SND_WND_MAX;
	return tcb;
}

TCB* tcb_find(uint32_t tcb_key) {
	TCB* tcb = map_get(tcbs, (void*)(uintptr_t)tcb_key);

	if(tcb == NULL) {
		printf("tcb is null \n");
		return NULL;
	}
	return tcb;
}

void tcp_set_callback(uint32_t socket, TCPCallback* callback, void* context) {
	TCB* tcb = tcb_find(socket);
	tcb->callback = callback;
}

TCPCallback* tcp_callback_create() {
	TCPCallback* callback = (TCPCallback*)malloc(sizeof(TCPCallback));
	return callback;
}

TCPCallback* tcp_get_callback(uint32_t socket) {
	TCB* tcb = tcb_find(socket);
	TCPCallback* callback;

	if(tcb->callback == NULL) {
		return tcp_callback_create();	
	} else {
		return tcb->callback;
	}
}

bool tcp_syn_send(void* context) {
	printf("tcp_syn_send in!!!\n");
	TCB* tcb = context;
		if(packet_out(tcb,1, 0, 0, 0, NULL) == true) {
			tcb->state = TCP_SYN_SENT; 
			printf("tcb state changed SYN_SENT\n");
			//printf("syn send success \n");
			return true;
		} else {
			tcb->state = TCP_CLOSED;
			//printf("syn send  did not success \n");
		}
	return true;
}

int tcp_connect(uint32_t dst_addr, uint16_t dst_port, TCPCallback* callback, void* context) {
	TCB* tcb;
	uint32_t tcb_key;
	NetworkInterface* ni = ni_get(0);
	arp_state = 0;
	
	if(tcbs == NULL)
		tcbs = map_create(4, map_uint64_hash, map_uint64_equals, NULL);
	
	tcb = tcb_create(ni, dst_addr, dst_port, callback);
	if(tcb == NULL) {
		printf("tcb NULL\n");
		return -1;
	} else {
		tcb_key = tcb_key_create(endian32(tcb->sip), endian16(tcb->sport));	
		if(!map_put(tcbs, (void*)(uintptr_t)tcb_key, (void*) tcb)) {
			return -1;
		}
	}
	ip_init_id();

	arp_get_mac_callback(ni, tcb->dip, tcb->sip, 3 * 1000000, (void*)tcb, tcp_syn_send);

	return tcb_key;
}

int tcp_send(uint32_t socket, const void* buf, size_t len) {
	TCB* tcb = tcb_find(socket);		
	packet_out(tcb, 0, 1, 1, 0, buf);
	tcb->callback->sent(socket, buf, len, NULL);	//TODO:sent() should be called when receive ack.
	return 1;	
}

IP* tcp_process(IP* ip) {
	uint32_t tcb_key;
	TCB* tcb;
	
	TCP* tcp = (TCP*)ip->body;
	tcb_key = tcb_key_create(ip->destination,tcp->destination);
	tcb = tcb_find(tcb_key);
						
	if(endian16(tcp->source) != tcb->dport) {
		return ip;
	}

	switch(tcb->state) {
		case TCP_CLOSED:
			printf("Connection is closed\n");
			break; 
		case TCP_SYN_SENT:
			if(tcp->syn == 1 && tcp->ack == 1) {
				printf("here\n");
				uint32_t sequence = tcp->sequence;	
				tcb->ackno = endian32(sequence)+1;
				//send ack
				if(packet_out(tcb, 0, 1, 0, 0, NULL)) {
					tcb->callback->connected(tcb_key, tcb->dip, tcb->dport, tcb->context);
					(tcb->ackno) = endian32(sequence)+1;
					tcb->state = TCP_ESTABLISHED;
					printf("tcb state changed ESTABLISHED \n");
				} 
				return ip;
			} else if (tcp->rst == 1) {
				//TODO: timeout
				tcb->state = TCP_CLOSED;
			}
			break;
		case TCP_SYN_RVCD:
			//TODO: server side	
			break;
		case TCP_ESTABLISHED:
			if(tcp->ack == 0) {
				//TODO: no logic decided 
			} else if(tcp->ack == 1) {
				if(tcp->psh == 1) {
					uint32_t sequence = tcp->sequence;	
					uint32_t acknowledge = tcp->acknowledgement;	
					uint32_t temp = endian32(acknowledge) - (tcb->seq); 
					(tcb->seq) = endian32(tcp->acknowledgement);	
					(tcb->ackno) = endian32(sequence)+temp;
					
					uint32_t idx = 0;
					uint32_t len = strlen((char*)tcp->payload);
					char value[len + 1];
					value[len] = '\0';
					memcpy(value, tcp->payload + idx, len);
					idx += len;
					tcb->callback->received(tcb_key,value,len +1,NULL);
					return ip;
				} else if(tcp->urg == 1) {
					//TODO: no logic decided 
				} else if(tcp->fin == 1) {
					//TODO: no logic decided 
				}		
			}
			break;
		case TCP_LISTEN:
			//TODO: no logic decided 
			break; 
		case TCP_CLOSING:
			//TODO: no logic decided 
			break;
		default:
			return ip;
	}
	return ip;
}


bool packet_out(TCB* tcb, int syn, int ack, int psh, int fin, const void* str) {
	printf("send_packet start \n");
	NetworkInterface* ni = ni_get(0);	
	Packet* packet;
	if(str != NULL) {	
		packet = ni_alloc(ni, sizeof(Ether) + sizeof(IP) + sizeof(TCP) + strlen(str)+1);
	} else {
		packet = ni_alloc(ni, sizeof(Ether) + sizeof(IP) + sizeof(TCP));
	}

	Ether* ether = (Ether*)(packet->buffer + packet->start);
	ether->dmac = endian48(arp_get_mac(ni,tcb->dip,tcb->sip));
        ether->smac = endian48(ni->mac);
        ether->type = endian16(ETHER_TYPE_IPv4);

	IP* ip = (IP*)ether->payload;
        ip->ihl = endian8(5);
        ip->version = endian8(4);
        ip->ecn = endian8(0); 
        ip->dscp = endian8(0);
	
		ip_id = ip_get_id(ack);
        ip->id = endian16(ip_id);
        ip->flags_offset = 0x40;
        ip->ttl = endian8(IP_TTL);
        ip->protocol = endian8(IP_PROTOCOL_TCP);
        ip->source = endian32(tcb->sip);
        ip->destination = endian32(tcb->dip);
	
	TCP* tcp = (TCP*)ip->body;
	tcp->source = endian16(tcb->sport);
	tcp->destination = endian16(tcb->dport);
	tcp_get_seqnum(psh, tcb);
	tcp->sequence = endian32(tcb->seq);
	tcp->acknowledgement = endian32(tcb->ackno);
	tcp->ns = endian8(0);
	tcp->reserved = endian8(0);
	tcp->offset = endian8(5);
	tcp->fin = endian8(fin); 
	tcp->syn = endian8(syn);
	tcp->rst = endian8(0);
	tcp->psh = endian8(psh);
	tcp->ack = endian8(ack);
	tcp->urg = endian8(0);
	tcp->ece = endian8(0);
	tcp->cwr = endian8(0);
	tcp->window = endian16(29200);
	tcp->urgent = endian16(0);
	
	if(str != NULL) {
		int str_len = strlen(str) + 1;
		uint16_t size = packet->size + str_len - strlen((char*)tcp->payload) + 1 ;
		uint16_t end = packet->end + str_len - strlen((char*)tcp->payload) + 1;
		memcpy(tcp->payload, str, strlen(str)+1);
		packet->size = size;
		packet->end = end; 
		ip->length = endian16(ip->ihl * 4 + tcp->offset *4 + str_len);
		tcp_pack(packet, str_len); 
	} else {
		packet->end = packet->start + sizeof(Ether) + sizeof(IP) + sizeof(TCP);
		ip->length = endian16(ip->ihl * 4 + tcp->offset *4 );
		tcp_pack(packet, TCP_LEN - 20);
	}

	return ni_output(ni, packet);
}

bool tcp_port_alloc0(NetworkInterface* ni, uint32_t addr, uint16_t port) {
	IPv4Interface* interface = ni_ip_get(ni, addr);
	if(!interface->tcp_ports) {
		interface->tcp_ports = set_create(64, set_uint64_hash, set_uint64_equals, ni->pool);
		if(!interface->tcp_ports)
			return false;
	}

	if(set_contains(interface->tcp_ports, (void*)(uintptr_t)port))
		return false;

	return set_put(interface->tcp_ports, (void*)(uintptr_t)port);
}

uint16_t tcp_port_alloc(NetworkInterface* ni, uint32_t addr) {
	IPv4Interface* interface = ni_ip_get(ni, addr);
	if(!interface->tcp_ports) {
		interface->tcp_ports = set_create(64, set_uint64_hash, set_uint64_equals, ni->pool);
		if(!interface->tcp_ports)
			return 0;
	}

	uint16_t port = interface->tcp_next_port;
	if(port < 49152)
		port = 49152;
	
	while(set_contains(interface->tcp_ports, (void*)(uintptr_t)port)) {
		if(++port < 49152)
			port = 49152;
	}	

	if(!set_put(interface->tcp_ports, (void*)(uintptr_t)port))
		return 0;
	
	interface->tcp_next_port = port;
	
	return port;
}

void tcp_port_free(NetworkInterface* ni, uint32_t addr, uint16_t port) {
	IPv4Interface* interface = ni_ip_get(ni, addr);
	if(interface == NULL)
		return;
	
	set_remove(interface->tcp_ports, (void*)(uintptr_t)port);
}

void tcp_pack(Packet* packet, uint16_t tcp_body_len) {
	Ether* ether = (Ether*)(packet->buffer + packet->start);
	IP* ip = (IP*)ether->payload;
	TCP* tcp = (TCP*)ip->body;
	
	uint16_t tcp_len = TCP_LEN + tcp_body_len;
	
	TCP_Pseudo pseudo;
	pseudo.source = ip->source;
	pseudo.destination = ip->destination;
	pseudo.padding = 0;
	pseudo.protocol = ip->protocol;
	pseudo.length = endian16(tcp_len);
	
	tcp->checksum = 0;
	uint32_t sum = (uint16_t)~checksum(&pseudo, sizeof(pseudo)) + (uint16_t)~checksum(tcp, tcp_len);
	while(sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	tcp->checksum = endian16(~sum);
	
	ip_pack(packet, tcp_len);
}
