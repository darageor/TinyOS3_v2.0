//edited for part 2.2

#ifndef __KERNEL_SOCKET_H
#define __KERNEL_SOCKET_H

/*****************************
 *
 *  The Socket Table    
 *
 *****************************/ 

#include "util.h"
#include "tinyos.h"
#include "bios.h"
#include "kernel_pipe.h"

typedef struct socket_control_block socket_cb;

/*Table with the ports of the sockets */
socket_cb* PORT_MAP[MAX_PORT+1] = {NULL};

typedef enum {
    SOCKET_LISTENER,
    SOCKET_UNBOUND,
    SOCKET_PEER
} socket_type;

typedef struct listener_socket{
	rlnode queue;
	CondVar req_available;
}listener_socket;

typedef struct unbound_socket{
	rlnode unbound_socket;
}unbound_socket;

typedef struct peer_socket{
	socket_cb* peer;
	pipe_cb* write_pipe;
	pipe_cb* read_pipe;
}peer_socket;


/**
  @brief Socket control block.

  These objects hold the information that is needed to 
  access a socket.
*/
typedef struct socket_control_block {

	uint refcount;

	FCB* fcb;
  	Fid_t fid;

	socket_type type;
	port_t port;
	
	union{
			listener_socket listener_s;
			unbound_socket unbound_s;
			peer_socket peer_s;
		};

} socket_cb;

typedef struct connection_request{
  /*(0,1): 1 if request was admitted*/
  int admitted;
  socket_cb* peer;

  CondVar connected_cv;
  rlnode queue_node;
} c_request;


/** @} */

#endif
