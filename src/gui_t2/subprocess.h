/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2015/2016
 *
 * subprocess.h
 *
 * Header file of functions that handle file transfer subprocesses and IPC with them
 *
 * Updated on September 25, 19:00
 * @author  Luis Bernardo, Rodolfo Oliveira
\*****************************************************************************/
extern char *out_dir;

#ifndef SUBPROCESS_INC_
#define SUBPROCESS_INC_

#include <gtk/gtk.h>


#include <netinet/in.h>
//#define DEBUG

// Pipe's message type
#define MSG_HEADING		1
#define MSG_TRANS		2
#define MSG_FIM_OK		3
#define MSG_FIM_INC		4
#define MSG_FIM_INTR		5
#define MSG_FIM_BADCONN		6
#define MSG_FIM_BADFILE		7
#define MSG_FIM_BADHDR		8
// Maximum message length
#define MAX_MESSAGE_LENGTH    5000

/************************************************************\
|* Functions that implement file transmission subprocesses  *|
\************************************************************/

// Sends a signal to a subprocess
void send_kill(int pid, int sig, gboolean may_fail);
// Callback of the  signal SIGCHLD in the main process
void reaper(int sig);

// Callback that receives data in the main process from the pipes connected to the subprocesses
gboolean callback_pipe(GIOChannel *chan, GIOCondition cond, gpointer ptr);

// Receives SIGUSR1 in a subprocess
void rcv_reaper(int sig);

// Receives SIGURG in a subprocess
void URG_reaper(int sig);

// Starts a subprocess for file reception
void start_rcv_child_process (int msgsock, const char *filename, gboolean optimal);

// Starts subprocess for sending a file
void start_file_upload_subprocess(struct in6_addr *ip_file, u_short porto, 
		const char *nome, const char *filename, gboolean optimal);


#endif
