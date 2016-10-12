#ifndef __NET_TCP_H__
#define __NET_TCP_H__

#include <net/ni.h>

/**
 * @file
 * Transmission Control Protocol verion 4
 */

#define TCP_LEN			20		///< TCPv4 header length

int32_t* debug_cur;
int32_t* debug_max;
//uint32_t debug_same;
/**
 * TCPv4 header
 */
typedef struct _TCP {
	uint16_t	source;			///< Source port number (endian16)
	uint16_t	destination;		///< Destination port number (endian16)
	uint32_t	sequence;		///< Sequence number (endian32)
	uint32_t	acknowledgement;	///< Acknowledgement number (endian32)
	uint8_t		ns: 1;			///< ECN-nonce concealment protection flag
	uint8_t		reserved: 3;		///< Reserved
	uint8_t		offset: 4;		///< Data offset, TCP header length in 32-bit words
	uint8_t		fin: 1;			///< No more data from sender flag
	uint8_t		syn: 1;			///< Synchronize sequence number flag
	uint8_t		rst: 1;			///< Reset the connection flag
	uint8_t		psh: 1;			///< Push flag
	uint8_t		ack: 1;			///< Acknowledgement field is significant flag
	uint8_t		urg: 1;			///< Urgent pointer is significant flag
	uint8_t		ece: 1;			///< ECN-Echo flag
	uint8_t		cwr: 1;			///< Congestion Window Reduced flag
	uint16_t	window;			///< Window size (endian16)
	uint16_t	checksum;		///< Header and data checksum (endian16)
	uint16_t	urgent;			///< Urgent pointer (endian16)
	
	uint8_t		payload[0];		///< TCP payload
} __attribute__ ((packed)) TCP;

/**
 * TCPv4 pseudo header to calculate header checksum
 */
typedef struct _TCP_Pseudo {
	uint32_t        source;			///< Source address (endian32)
	uint32_t        destination;		///< Destination address (endian32)
	uint8_t         padding;		///< Zero padding
	uint8_t         protocol;		///< TCP protocol number, 0x06
	uint16_t        length;			///< Header and data length in bytes (endian32)
} __attribute__((packed)) TCP_Pseudo;

typedef int (*connected)(uint32_t socket, uint32_t addr, uint16_t port, void* context);
typedef int (*bound)(uint32_t socket, uint32_t addr, uint16_t port, void* context);
typedef int (*disconnected)(uint32_t socket);
typedef int (*sent)(uint32_t socket, size_t len, void* context);
typedef int (*received)(uint32_t socket, const void* buf, size_t len, void* context);

typedef struct {
	connected connected;
	bound bound;
	disconnected disconnected;
	sent sent;
	received received;
} TCPCallback;

/**
  * Init valuse about tcp.
  *
  * @return false if initiation fail, else true.
  */
bool tcp_init();

/**
  * TCP timer function, need to be added using event_add().
  */
bool tcp_timer(void* context);

/**
 * Process all TCP packet.
 *
 * @param packet Packet 
 * @return true if there is no error, else return false 
 */
bool tcp_process(Packet* packet);

/**
 * Get callbacks pointer of socket.
 *
 * @param socket socket number
 * @return NULL if fail, else TCPCallback pointer and make new pointer if there was no callback 
 */
TCPCallback* tcp_get_callback(int32_t socket);

/**
 * Set callbacks pointer of socket.
 *	
 * @param socket socket number
 * @param callback new callbacks to set
 * @param context callback's context
 * @return NULL if fail, else TCPCallback pointer and make new pointer if there was no callback 
 */
void tcp_set_callback(int32_t socket, TCPCallback* callback, void* context);

/**
 * Allocate Memory to hold callback functions.
 *
 * @return NULL if fail, else TCPCallback pointer 
 */
TCPCallback* tcp_callback_create();

/**
 * Connect to remote computer, Send SYN packet.
 *
 * @param ni NetworkInterface that IP is added.
 * @param dst_addr remote computer's IP address
 * @param dst_port remote computer's port
 * @param callback this connection's callback functions
 * @return < 0 if error occurred, else socket number used tcb_key internally
 */
uint32_t tcp_connect(NetworkInterface* ni, uint32_t dst_addr, uint16_t dst_port, TCPCallback* callback, void* context);

/**
 * Send data.
 *
 * @param socket TCP socket 
 * @param data data's pointer
 * @param len data's length
 * @return -1 if fail to send, else sent data size
 */
int32_t tcp_send(uint32_t socket, void* data, const int32_t len);

bool tcp_close(uint32_t socket);

/**
 * Allocate TCP port number which associated with NI.
 *
 * @param ni NI reference
 * @param addr
 * @param port
 * @return true if port alloc success
 */
bool tcp_port_alloc0(NetworkInterface* ni, uint32_t addr, uint16_t port);

/**
 * Allocate TCP port number which associated with NI.
 *
 * @param ni NI reference
 * @param addr
 * @return port number
 */
uint16_t tcp_port_alloc(NetworkInterface* ni, uint32_t addr);

/**
 * Free TCP port number.
 *
 * @param ni NI reference
 * @param addr
 * @param port port number to free
 */
void tcp_port_free(NetworkInterface* ni, uint32_t addr, uint16_t port);

/**
 * Set TCP checksum, and do IP packing.
 *
 * @param packet TCP packet to pack
 * @param tcp_body_len TCP body length in bytes
 */
void tcp_pack(Packet* packet, uint16_t tcp_body_len);

#endif /* __NET_TCP_H__ */
