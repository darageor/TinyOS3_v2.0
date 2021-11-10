//edited for part 2.2

#include <assert.h>
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_socket.h"
#include "kernel_pipe.h"
#include "kernel_dev.h"
#include "kernel_cc.h"

void* socket_open(uint term){
	return NULL; 
}

int  socket_read(void* sock, char *buf, unsigned int n){
	socket_cb* sockettable = (socket_cb*) sock;

	if(sockettable == NULL) return -1;
	
	if(sockettable->type == SOCKET_PEER){
		int ret = pipe_read(sockettable->peer_s.read_pipe, buf, n);
		return ret;
	}else{
		return -1;
	}
}

int socket_write(void* sock, const char *buf, unsigned int n){
	socket_cb* sockettable = (socket_cb*) sock;

	if(sockettable == NULL) return -1;

	if(sockettable->type == SOCKET_PEER){
		int ret = pipe_write(sockettable->peer_s.write_pipe, buf, n);
		return ret;
	}else{
		return -1;
	}
}

int socket_close(void* sock){
	socket_cb* sockettable = (socket_cb*) sock;

	if(sockettable == NULL) return -1;
	
	/*close a peer socket*/
	if(sockettable->type == SOCKET_PEER){
		/*Close the pipes of the reader and writer*/
		pipe_reader_close(sockettable->peer_s.read_pipe);
		pipe_writer_close(sockettable->peer_s.write_pipe);

		/*Decrease the reference counter*/
		sockettable->refcount--;

		/*if the socket is connected with an other socket 
		destroy the connection*/
		if(sockettable->peer_s.peer != NULL){
			sockettable->peer_s.peer->peer_s.peer = NULL;
		}
		
	}else if(sockettable->type == SOCKET_LISTENER){
		/*Make the socket unbound*/
		PORT_MAP[sockettable->port] = NULL;

		/*Decrease the reference counter*/
		sockettable->refcount--;

		/*Wake up the listener with available request */
		kernel_signal(&sockettable->listener_s.req_available);

		/*While the request queue is not empty*/
		while(!is_rlist_empty(&sockettable->listener_s.queue)){

			/*Take the first request from the queue*/
			rlnode* request = rlist_pop_front(&sockettable->listener_s.queue);

			/*Wake up sockets with the connection request cv*/
			kernel_signal(&request->req_queue->connected_cv);
		}
	}

	if(sockettable->refcount == 0) free(sockettable);

	return 0;
}


static file_ops socket_fops = {
  .Open = socket_open,
  .Read = socket_read,
  .Write = socket_write,
  .Close = socket_close
};

Fid_t sys_Socket(port_t port){

	Fid_t socket_fid; 
	FCB* socket_fcb;
	if(port>=0 && port<=MAX_PORT){

		if (FCB_reserve(1,&socket_fid,&socket_fcb)){
			socket_cb* sockettable = xmalloc(sizeof(socket_cb));

			socket_fcb->streamfunc = &socket_fops;
			socket_fcb->streamobj = sockettable;

			sockettable->fcb = socket_fcb;
			sockettable->fid = socket_fid;

			//mporei na prepei na arxikopoih8ei se 1 ???
			sockettable->refcount = 1;

			sockettable->type = SOCKET_UNBOUND;
			sockettable->port = port;

			return socket_fid;
		}
	}	
	return NOFILE;
}

int sys_Listen(Fid_t sock)
{
	if(sock < 0 || sock > MAX_FILEID) //the file id is not legal
		return -1;
	
	FCB* fcb = get_fcb(sock);
	
	if(fcb == NULL)
		return -1; 

	socket_cb* scb = fcb->streamobj;

	if(scb == NULL)
		return -1;

	if(scb->port > 0 && scb->port < MAX_PORT){
		if(scb->type == SOCKET_UNBOUND){
			if(PORT_MAP[scb->port] == NULL){
				
				PORT_MAP[scb->port] = scb;
				scb->type = SOCKET_LISTENER;

				scb->listener_s.req_available = COND_INIT;
				rlnode_init(&scb->listener_s.queue, NULL);
				return 0;
			}
		}
	}
	return -1;
}


Fid_t sys_Accept(Fid_t lsock)
{
	if(lsock < 0 || lsock >= MAX_FILEID)
		return NOFILE;

	FCB* fcb = get_fcb(lsock);

	if(fcb == NULL)
		return NOFILE;

	socket_cb* scb = fcb->streamobj;

	if(scb == NULL)
		return NOFILE;

	if(scb->port >= 0 && scb->port <= MAX_PORT){
		if(scb->type == SOCKET_LISTENER){
			if(PORT_MAP[scb->port] != NULL){
				scb->refcount++;
				
				while(is_rlist_empty(&scb->listener_s.queue)){
					kernel_wait(&scb->listener_s.req_available, SCHED_PIPE);

					if(PORT_MAP[scb->port] == NULL){
					//kernel_signal
					return NOFILE;
				}
				}

				if(PORT_MAP[scb->port] == NULL){
					//kernel_signal
					return NOFILE;
				}

				rlnode* c_request = rlist_pop_front(&scb->listener_s.queue);

				socket_cb* request_scb = c_request->req_queue->peer;
				FCB* request_fcb = request_scb->fcb;

				Fid_t accept_fid = sys_Socket(scb->port);

				FCB* accept_fcb = get_fcb(accept_fid);

				if(accept_fcb == NULL){
					return NOFILE;
				}

				socket_cb* accept_scb = accept_fcb->streamobj;
				
				if(accept_scb == NULL){
					return NOFILE;
				}

				request_scb->type = SOCKET_PEER;
				accept_scb->type = SOCKET_PEER;


				//connect the 2 sockets

				//initialize 2 pipes
				FCB* fcb1[2];
				fcb1[0] = request_fcb;
				fcb1[1] = accept_fcb;

				pipe_cb* accept_pipe = initialize_pipes(fcb1);

				FCB* fcb2[2];
				fcb2[0] = accept_fcb;
				fcb2[1] = request_fcb;
     			pipe_cb* request_pipe = initialize_pipes(fcb2);

				//Check if Pipes initialized
				if(accept_pipe == NULL){
					return NOFILE;
				}

				if(request_pipe == NULL){
					return NOFILE;
				}

				//connect the 2 pipes
				request_scb->peer_s.write_pipe = request_pipe;
				request_scb->peer_s.read_pipe = accept_pipe;
				accept_scb->peer_s.write_pipe = accept_pipe;
				accept_scb->peer_s.read_pipe = request_pipe;

				request_scb->peer_s.peer = accept_scb;
				accept_scb->peer_s.peer = request_scb;

				c_request->req_queue->admitted = 1;

				kernel_signal(&c_request->req_queue->connected_cv);

				return accept_fid;
			}
		}
	}

return NOFILE;
}

int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	if(port<=0 || port>MAX_PORT) return -1; 

	if(sock < 0 || sock >= MAX_FILEID) return -1;

	FCB* fcb = get_fcb(sock);
	if(fcb==NULL) return -1;

	socket_cb* client = fcb->streamobj;

	if(client==NULL) return -1;

	if(client->type != SOCKET_UNBOUND) return -1;

	if(PORT_MAP[port] == NULL) return -1;

	socket_cb* listener = PORT_MAP[port];
	listener->fid = sock;

	if(listener->type != SOCKET_LISTENER) return -1;
	if(listener == client) return -1;

	c_request* con_req = (c_request*)xmalloc(sizeof(c_request));
	con_req->peer = client;
 	con_req->admitted = 0;
 	con_req->connected_cv = COND_INIT;

	rlnode_init(&con_req->queue_node, con_req);

 	rlist_push_back(&listener->listener_s.queue, &con_req->queue_node);
	listener->refcount++;
	

	kernel_signal(&listener->listener_s.req_available);

	int ret_val = kernel_timedwait(&con_req->connected_cv, SCHED_USER, timeout);
	

	rlist_remove(&con_req->queue_node);
    
	listener->refcount--;
	if(ret_val == 1){
		if(con_req->admitted == 1){
			return 0;
		}
	}
	return -1;
}

int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	FCB* sock_fcb = get_fcb(sock);
	socket_cb* sockettable = sock_fcb->streamobj;
	int retval = -1;

	if(how == SHUTDOWN_READ){
		if(sockettable->type == SOCKET_PEER){
			retval = pipe_reader_close(sockettable->peer_s.read_pipe);

		}

	}else if(how == SHUTDOWN_WRITE){
		if(sockettable->type == SOCKET_PEER){
			retval = pipe_writer_close(sockettable->peer_s.write_pipe);

		}
	}else if(how == SHUTDOWN_BOTH){

		if(sockettable->type == SOCKET_PEER){
			int retval_r = pipe_reader_close(sockettable->peer_s.read_pipe);
			int retval_w = pipe_writer_close(sockettable->peer_s.write_pipe);

			if(retval_r == 0 && retval_w == 0) retval = 0;
		}

	}

	return retval;
}

