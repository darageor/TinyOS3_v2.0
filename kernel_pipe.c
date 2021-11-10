//edited for part 2.1

#include <assert.h>
#include "kernel_dev.h"
#include "kernel_streams.h"
#include "kernel_cc.h"
#include "kernel_pipe.h"
#include "tinyos.h"


static file_ops pipe_writer_fops = {
  .Open = pipe_open,
  .Read = pipe_writer_read,
  .Write = pipe_write,
  .Close = pipe_writer_close
};

static file_ops pipe_reader_fops = {
  .Open = pipe_open,
  .Read = pipe_read,
  .Write = pipe_reader_write,
  .Close = pipe_reader_close
};

void* pipe_open(uint term){
  return NULL; 
}


/*************************Reader-Ops**********************/
int  pipe_read(void* pipecb_t, char *buf, unsigned int n){

  pipe_cb* pipe = (pipe_cb*) pipecb_t;
  uint i=0;

  /*if the writer has closed return ERROR*/
  if(pipe->reader == NULL){			
		return -1; 
	}

  /*if the writer has closed and the buffer is empty return 0*/
  if(pipe->writer == NULL && pipe->r_position == pipe->w_position){			
	  	return 0; 
	}

  for(i=0; i<n; i++){

	  /*If the buffer is empty awake the writer and wait*/
	  while(pipe->r_position == pipe->w_position){

	  	  /*if the writer has closed return the amount you have read*/
	  	  if(pipe->writer == NULL){			
	  	  		  
				  return i; 
				}

		  kernel_broadcast(&pipe->has_space);
		  kernel_wait(&pipe->has_data, SCHED_PIPE);
	  }

  	/*if the reader has closed after awake return the amount you have read*/
  	if(pipe->reader == NULL){		
	  	  return i; 
	  }

	  buf[i] = pipe->BUFFER[pipe->r_position];
	  pipe->r_position++;
	  
  	 if(pipe->r_position == PIPE_BUFFER_SIZE){
		  pipe->r_position = 0;
	  }
	  
  }
  
  kernel_broadcast(&pipe->has_space);
  
  return i;  
}

int pipe_reader_write(void* pipecb_t, const char *buf, unsigned int n){
	/*The reader can't write.
	Return ERROR if called*/
	return -1; 
}

int pipe_reader_close(void* pipecb_t){

	pipe_cb* pipe = (pipe_cb*) pipecb_t;

    /*If pipe does not exist return ERROR */
    if(pipe == NULL ){
    	return -1;
    }

    kernel_broadcast(&pipe->has_space);

    /* If the reader is closed already return*/
	if (pipe->reader == NULL){
		return 0;
	}

	/*Close the reader*/
    pipe->reader = NULL;

    /*If the writer has closed free the pipe*/
    if(pipe->writer == NULL){
    	free(pipe);
    }

	return 0; 
}

/*************************Writer-Ops**********************/
int pipe_write(void* pipecb_t, const char *buf, unsigned int n){
  
  pipe_cb* pipe = (pipe_cb*) pipecb_t;
  uint i=0;
  int checkpoint = 0;
  
  /*if the reader or the writer are close return ERROR*/
  if(pipe->reader == NULL || pipe->writer == NULL){
		return -1;
	}

  	for(i=0; i<n; i++){

		/*By using the checkpoint we check if the buffer is full*/
		checkpoint = pipe->r_position-1;
		if(checkpoint < 0){ 
				checkpoint = PIPE_BUFFER_SIZE-1;
			}

		/*If the buffer is full awake the reader and wait*/
		while(pipe->w_position  == checkpoint ){

			/*if reader closed before awake return ERROR*/
			if(pipe->reader == NULL){
				return -1;
			}

			/*if writer closed before awake return the writen amount*/
			if(pipe->writer == NULL){		
				return i;
			}

			kernel_broadcast(&pipe->has_data);
			kernel_wait(&pipe->has_space, SCHED_PIPE);

			/*By using the checkpoint we check if the buffer is full*/
			checkpoint = pipe->r_position-1;
			if(checkpoint < 0){ 
				checkpoint = PIPE_BUFFER_SIZE-1;
			}

		}

		/*if reader closed after awake return ERROR*/
		if(pipe->reader == NULL){		
			return -1;
		}

		/*if writer closed after awake return the writen amount*/
		if(pipe->writer == NULL){	
			return i;
		}

		pipe->BUFFER[pipe->w_position] = buf[i];
		pipe->w_position++;

		if(pipe->w_position == PIPE_BUFFER_SIZE){
			pipe->w_position = 0;
		}

	}

  kernel_broadcast(&pipe->has_data);

  return i; 
}

int pipe_writer_read(void* pipecb_t, char *buf, unsigned int n){
	/*The writer can't read.
	Return ERROR if called*/
	return -1; 
}

int pipe_writer_close(void* pipecb_t){
	
    pipe_cb* pipe = (pipe_cb*) pipecb_t;

    /*If pipe does not exist return ERROR */
    if(pipe == NULL ){
    	return -1;
    }

    kernel_broadcast(&pipe->has_data);

    /* If the writer is closed already return*/
	if (pipe->writer == NULL){
		return 0;
	}

	/*Close the writer*/
	pipe->writer = NULL;

    /*If the reader has closed free the pipe*/
    if(pipe->reader == NULL){
    	free(pipe);
    }

	return 0; 
}

/*************************Initialization**********************/
int sys_Pipe(pipe_t* pipe){
	/*We create two arrays one with file ids and one with fcbs to 
	create the connection in the fileid table of the process*/
	Fid_t pipe_fid[2]; 
	FCB* pipe_fcb[2];
	if (FCB_reserve(2,pipe_fid,pipe_fcb)){

		pipe->read = pipe_fid[0];
		pipe->write = pipe_fid[1];

		pipe_cb* pipetable = initialize_pipes(pipe_fcb);

		pipe_fcb[0]->streamfunc = &pipe_reader_fops;	//the first place of the fcb list points to the reader file_ops
		pipe_fcb[1]->streamfunc = &pipe_writer_fops;	//the second place of the fcb list points to the writer file_ops

		/*Both reader and writer have the same pipe controle block*/
		pipe_fcb[0]->streamobj = pipetable;		
		pipe_fcb[1]->streamobj = pipetable;

		return 0;
	}
	return -1;
}


pipe_cb* initialize_pipes(FCB* pipe_fcb[2]){

	pipe_cb* pipetable = xmalloc(sizeof(pipe_cb));
	/*initialize the pipe controle block*/
	pipetable->has_space = COND_INIT;
	pipetable->has_data = COND_INIT;
	pipetable->w_position = 0;
	pipetable->r_position = 0;

	pipetable->reader = pipe_fcb[0];
	pipetable->writer = pipe_fcb[1];

	return pipetable;
}


