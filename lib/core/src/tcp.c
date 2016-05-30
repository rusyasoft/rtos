#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <gmalloc.h>
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

	List* seg_unack;	// sent segment but no ack.
	uint16_t snd_wnd_max;
	uint16_t snd_wnd_cur;
	uint16_t recv_wnd_max;
	uint16_t recv_wnd_cur;
	NetworkInterface* ni;
} TCB;

typedef struct {
	uint32_t seq;	// host endian
	uint8_t syn: 1;
	uint8_t ack: 1;
	uint8_t psh: 1;
	uint8_t fin: 1;

	uint16_t len;
	void* data;
} Segment;

static uint32_t ip_id;
static int arp_state;
static Map* tcbs = NULL;

static bool packet_out(TCB* tcb, uint8_t syn, uint8_t ack, uint8_t psh, uint8_t fin, const void* data, int len); 

static uint32_t tcp_init_seqnum() {
	uint64_t time;
	uint32_t* p = (uint32_t*)&time;
	asm volatile("rdtsc" : "=a"(p[0]), "=d"(p[1]));
	return time % MAX_SQUENCENUM;
}

/**
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
**/

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
	
	tcb->seg_unack = list_create(NULL);
	tcb->recv_wnd_max = RECV_WND_MAX;
	tcb->recv_wnd_cur = RECV_WND_MAX;
	tcb->snd_wnd_max = SND_WND_MAX;
	tcb->snd_wnd_cur = SND_WND_MAX;

	tcb->ni = ni;
	return tcb;
}

static bool tcb_destroy(TCB* tcb) {
	tcp_port_free(tcb->ni, ADDRESS, tcb->sport);
	
	ListIterator iter;
	list_iterator_init(&iter, tcb->seg_unack);

	while(list_iterator_has_next(&iter)) {
		Segment* seg = list_iterator_next(&iter);
		gfree(seg->data);
		list_iterator_remove(&iter);
		gfree(seg);
	}

	list_destroy(tcb->seg_unack);
	
	uint32_t tcb_key = tcb_key_create(endian32(tcb->sip), endian16(tcb->sport));
	void* result = map_remove(tcbs, (void*)(uintptr_t)tcb_key);
	if(!result)
		printf("map_remove error\n");

	free(tcb);

	return true;
}

TCB* tcb_find(uint32_t tcb_key) {
	TCB* tcb = map_get(tcbs, (void*)(uintptr_t)tcb_key);

	if(tcb == NULL) {
		printf("tcb is null \n");
		return NULL;
	}
	return tcb;
}

void tcp_set_callback(int32_t socket, TCPCallback* callback, void* context) {
	TCB* tcb = tcb_find(socket);
	tcb->callback = callback;
}

TCPCallback* tcp_callback_create() {
	return (TCPCallback*)malloc(sizeof(TCPCallback));
}

TCPCallback* tcp_get_callback(int32_t socket) {
	TCB* tcb = tcb_find(socket);

	if(tcb->callback == NULL) {
		return tcp_callback_create();	
	} else {
		return tcb->callback;
	}
}

bool tcp_syn_send(void* context) {
	TCB* tcb = context;
	
	if(packet_out(tcb,1, 0, 0, 0, NULL, 0)) {
		tcb->state = TCP_SYN_SENT; 
		tcb->seq++;
		printf("tcb state changed SYN_SENT\n");
		
		return true;
	} else {
		tcb->state = TCP_CLOSED;
		
		return false;
	}
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
			printf("map_put error\n");
			return -1;
		}
	}
	ip_init_id();
	printf("before arp\n");
	uint64_t mac = arp_get_mac_callback(ni, tcb->dip, tcb->sip, 3 * 1000000, (void*)tcb, tcp_syn_send);
	printf("after arp\n");
	
	if(mac != 0xffffffffffff)
		tcp_syn_send(tcb);

	return tcb_key;
}

int32_t tcp_close(int32_t socket) {
	TCB* tcb = tcb_find(socket);
	if(!tcb)
		return -1;

	if(!packet_out(tcb, 0, 1, 0, 1, NULL, 0))
		return -1;
	
	tcb->state = TCP_FIN_WAIT_1;
	tcb->seq += 1;

	return 0;
}

int tcp_send(int32_t socket, const void* buf, size_t len) {
	TCB* tcb = tcb_find(socket);
	if(!tcb)
		return -1;
	
	if(tcb->snd_wnd_cur < len)
		len = tcb->snd_wnd_cur;

	tcb->snd_wnd_cur -= len;
	
	if(!packet_out(tcb, 0, 1, 1, 0, buf, len)) {
		tcb->snd_wnd_cur += len;
		return -1;
	}
	
	tcb->seq = tcb->seq + len;

	return len;	
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
				tcb->ackno = endian32(tcp->sequence) + 1;
				tcb->snd_wnd_max = endian16(tcp->window);
				tcb->snd_wnd_cur = endian16(tcp->window);

				if(packet_out(tcb, 0, 1, 0, 0, NULL, 0)) {	// send ack
					tcb->state = TCP_ESTABLISHED;
					tcb->callback->connected(tcb_key, tcb->dip, tcb->dport, tcb->context);
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
			if(tcp->ack == 1) {
				tcb->snd_wnd_max = endian16(tcp->window);
				ListIterator iter;
				list_iterator_init(&iter, tcb->seg_unack);

				while(list_iterator_has_next(&iter)) {
					Segment* seg = list_iterator_next(&iter);

					if(seg->seq < tcp->acknowledgement) {
						list_iterator_remove(&iter);
						tcb->snd_wnd_cur += seg->len;							

						if(tcb->snd_wnd_cur > tcb->snd_wnd_max)
							tcb->snd_wnd_cur = tcb->snd_wnd_max;
						tcb->callback->sent(tcb_key, seg->data, seg->len, tcb->context);

						gfree(seg->data);
						gfree(seg);
					}
				}

				uint16_t len = endian16(ip->length) - ip->ihl * 4 - tcp->offset * 4;
				if(len > 0 && tcb->ackno == endian32(tcp->sequence)) {
					tcb->ackno += len;

					packet_out(tcb, 0, 1, 0, 0, NULL, 0);	//send ack

					tcb->callback->received(tcb_key, (uint8_t*)tcp + tcp->offset * 4, len, tcb->context);	// TODO: check last arg(context).
				}

			} else {
				//TODO: no logic decided
			}
			break;
		case TCP_LISTEN:
			//TODO: no logic decided 
			break;
		case TCP_FIN_WAIT_1:
			if(tcp->fin && tcp->ack) {
				ListIterator iter;
				list_iterator_init(&iter, tcb->seg_unack);

				while(list_iterator_has_next(&iter)) {
					Segment* seg = list_iterator_next(&iter);

					if(seg->seq < tcp->acknowledgement) {
						list_iterator_remove(&iter);
						tcb->snd_wnd_cur += seg->len;

						if(tcb->snd_wnd_cur > tcb->snd_wnd_max)
							tcb->snd_wnd_cur = tcb->snd_wnd_max;
						tcb->callback->sent(tcb_key, seg->data, seg->len, tcb->context);

						gfree(seg->data);
						gfree(seg);
					}
				}

				uint16_t len = endian16(ip->length) - ip->ihl * 4 - tcp->offset * 4;
				if(len > 0 && tcb->seq >= endian32(tcp->acknowledgement)) {
					tcb->ackno += len;

					packet_out(tcb, 0, 1, 0, 0, NULL, 0);	//send ack

					tcb->callback->received(tcb_key, (uint8_t*)tcp + tcp->offset * 4, len, tcb->context);
				} else if(tcb->ackno <= endian32(tcp->sequence)) {
					tcb->ackno++;

					packet_out(tcb, 0, 1, 0, 0, NULL, 0);	//send ack
					
					tcb->state = TCP_CLOSING;
				}

				if(tcb->seq == endian32(tcp->acknowledgement)) {
					tcb->state = TCP_CLOSING;
					tcb->state = TCP_TIME_WAIT;
					tcb->state = TCP_CLOSED;
					
					tcb_destroy(tcb);

				}
			} else if(tcp->ack) {
				ListIterator iter;
				list_iterator_init(&iter, tcb->seg_unack);

				while(list_iterator_has_next(&iter)) {
					Segment* seg = list_iterator_next(&iter);

					if(seg->seq < tcp->acknowledgement) {
						list_iterator_remove(&iter);
						tcb->snd_wnd_cur += seg->len;

						if(tcb->snd_wnd_cur > tcb->snd_wnd_max)
							tcb->snd_wnd_cur = tcb->snd_wnd_max;
						tcb->callback->sent(tcb_key, seg->data, seg->len, tcb->context);

						gfree(seg->data);
						gfree(seg);
					}
				}

				uint16_t len = endian16(ip->length) - ip->ihl * 4 - tcp->offset * 4;
				if(len > 0 && tcb->seq >= endian32(tcp->acknowledgement)) {
					tcb->ackno += len;

					packet_out(tcb, 0, 1, 0, 0, NULL, 0);	//send ack

					tcb->callback->received(tcb_key, (uint8_t*)tcp + tcp->offset * 4, len, tcb->context);
				}

				if(tcb->seq == endian32(tcp->acknowledgement)) {
					tcb->state = TCP_FIN_WAIT_2;
				}
			}
			break;

		case TCP_FIN_WAIT_2:
			if(tcp->ack) {
				ListIterator iter;
				list_iterator_init(&iter, tcb->seg_unack);

				while(list_iterator_has_next(&iter)) {
					Segment* seg = list_iterator_next(&iter);

					if(seg->seq < tcp->acknowledgement) {
						list_iterator_remove(&iter);
						tcb->snd_wnd_cur += seg->len;

						if(tcb->snd_wnd_cur > tcb->snd_wnd_max)
							tcb->snd_wnd_cur = tcb->snd_wnd_max;
						tcb->callback->sent(tcb_key, seg->data, seg->len, tcb->context);

						gfree(seg->data);
						gfree(seg);
					}
				}
				
				uint16_t len = endian16(ip->length) - ip->ihl * 4 - tcp->offset * 4;
				if(len > 0 && tcb->ackno == endian32(tcp->sequence)) {
					tcb->ackno += len;

					tcb->state = TCP_CLOSED;
					packet_out(tcb, 0, 1, 0, 0, NULL, 0);	//send ack

					tcb->callback->received(tcb_key, (uint8_t*)tcp + tcp->offset * 4, len, tcb->context);	// TODO: check last arg(context).
				}
				
				if(tcp->fin && tcb->seq == endian32(tcp->acknowledgement)) {
					tcb->ackno = endian32(tcp->sequence) + 1;
					packet_out(tcb, 0, 1, 0, 0, NULL, 0); //send ack
					tcb->state = TCP_TIME_WAIT;	//need to implement time wait
					tcb->state = TCP_CLOSED;
					tcb_destroy(tcb);
				}
			}
			break;
		case TCP_CLOSING:
			if(tcp->ack) {
				if(tcb->seq == endian32(tcp->acknowledgement)) {
					tcb->state = TCP_TIME_WAIT;	//need to implement time wait
					tcb->state = TCP_CLOSED;
					tcb_destroy(tcb);
				}
			}
			//TODO: no logic decided 
			break;
		default:
			return ip;
	}
	return ip;
}

// TODO: function naming. and maybe need some modulization.
static bool packet_out(TCB* tcb, uint8_t syn, uint8_t ack, uint8_t psh, uint8_t fin, const void* data, int len) {
	NetworkInterface* ni = ni_get(0);
	Packet* packet = ni_alloc(ni, sizeof(Ether) + sizeof(IP) + sizeof(TCP) + len);
	
	if(!packet)
		return false;

	if(len != 0) {
		Segment* segment = gmalloc(sizeof(Segment));
		segment->seq = tcb->seq;
		segment->syn = endian8(syn);
		segment->ack = endian8(ack);
		segment->psh = endian8(psh);
		segment->fin = endian8(fin);
		segment->len = len;
		segment->data = gmalloc(len);
		
		memcpy(segment->data, data, len);
		list_add(tcb->seg_unack, segment);
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
	tcp->window = endian16(tcb->recv_wnd_cur);
	tcp->urgent = endian16(0);
	memcpy(tcp->payload, data, len);
	tcp_pack(packet, len);

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
