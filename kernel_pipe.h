//edited for part 2.1

#ifndef __KERNEL_PIPE_H
#define __KERNEL_PIPE_H

/*****************************
 *
 *  The Pipe Table    
 *
 *****************************/ 

#include "util.h"
#include "tinyos.h"
#include "bios.h"

/** @brief Maximum number data in a pipe. */
#define PIPE_BUFFER_SIZE 8192 // 2^13 

/**
  @brief Pipe control block.

  These objects hold the information that is needed to 
  access a pipe.
*/
typedef struct pipe_control_block {

	FCB *reader, *writer;
	
	CondVar has_space;    /* For blocking writer if no space is available */
	CondVar has_data;     /* For blocking reader until data are available */
	
	int w_position, r_position;  /* write, read position in buffer (it depends on your implementation of bounded buffer, i.e. alternatively pointers can be used)*/

  int read_amount,write_amount;
	char BUFFER[PIPE_BUFFER_SIZE];   /* bounded (cyclic) byte buffer */

	
} pipe_cb;


/** 
  @brief Open a pipe.
 */
void* pipe_open(uint term);

/** 
  @brief Read from pipe.
 */
int  pipe_read(void* pipecb_t, char *buf, unsigned int n);


/** 
  @brief Reader cant write in a pipe.
 */
int pipe_reader_write(void* pipecb_t, const char *buf, unsigned int n);


/** 
  @brief Reader closes the pipe.
 */
int pipe_reader_close(void* _pipecb);


/** 
  @brief Write in a pipe.
 */
int pipe_write(void* pipecb_t, const char *buf, unsigned int n);

/** 
  @brief Writer cant read from pipe.
 */
int pipe_writer_read(void* pipecb_t, char *buf, unsigned int n);


/** 
  @brief Writer closes the pipe.
 */
int pipe_writer_close(void* _pipecb);


/** 
  @brief Initialization for pipes.
 */
pipe_cb* initialize_pipes(FCB* pipe_fcb[2]);

/** @} */

#endif