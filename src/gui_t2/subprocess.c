/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2015/2016
 *
 * subprocess.c
 *
 * Functions that implement file transfer subprocesses and IPC
 *
 * Updated on September 25, 19:00
 * @author  Luis Bernardo, Rodolfo Oliveira
\*****************************************************************************/
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/time.h>
#include <assert.h>
#include <netinet/in.h>
#include <signal.h>       
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>


#include "subprocess.h"
#include "callbacks.h"
#include "sock.h"
#include "file.h"
#include "gui.h"

#ifdef DEBUG
#define debugstr(x)     g_print("%s", x)
#else
#define debugstr(x)
#endif


// Temporary buffer
static char tmp_buf[8000];



/************************************************************\
|* Functions that implement file transmission subprocesses  *|
\************************************************************/

// Global variables used in the subprocesses - some are set in the main process and read in the subprocess
static int s;	// Descriptor of the TCP socket
static char name_str[80];
static gboolean signal_ev= FALSE;	// TRUE when a signal is received
static int sp; 			// Pipe descriptor
static long total= 0; // Bytes handled in the subprocess
static long flen= 0;  // File length


// Sends a signal to a subprocess
void send_kill(int pid, int sig, gboolean may_fail) {
	assert(sig>0);
	assert(pid>0);

	char nome[80];
	switch(sig) {
	case SIGKILL:
		sprintf(nome, "SIGKILL");
		break;
	case SIGUSR1:
		sprintf(nome, "SIGUSR1");
		break;
	default:
		sprintf(nome, "%d", sig);
		break;
	}

	if (!kill(pid, sig)) {
#ifdef DEBUG  
		sprintf(tmp_buf, "Sent Kill(%s) to subprocess %d\n", nome, pid);
		Log(tmp_buf);
#endif
	} else {
		if (!may_fail) {
			sprintf(tmp_buf, "Kill(%s) to subprocess %d failed: %s\n",
					nome, pid, strerror(errno));
			Log(tmp_buf);
		}
	}
}


// Callback of the  signal SIGCHLD in the main process
void reaper(int sig)
{
	sigset_t set, oldset;
	pid_t pid;
	union wait status;
	gboolean free_mem= TRUE;

#ifdef DEBUG
	fprintf(stderr, "reaper started\n");
#endif  
	// Reinstall SIGCHLD signal handler
	//signal(SIGCHLD, reaper);
	// blocks other SIGCHLD signals
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigprocmask(SIG_BLOCK, &set, &oldset);

	while ((pid= wait3(&status, WNOHANG, 0)) > 0) {
		if (WIFEXITED(status)) {
			fprintf(stderr, "Subprocess (pid= %d) ended with exit(%d)\n",
					(int)pid, (int)WEXITSTATUS(status));
			if (WEXITSTATUS(status) == 0)  // Exit code was 0
				free_mem= FALSE;
		} else if (WIFSIGNALED(status))
			fprintf(stderr, "Subprocess (pid= %d) ended with kill(%d)\n",
					(int)pid, (int)WTERMSIG(status));
		else
			fprintf(stderr, "Subprocess (pid= %d) ended\n", (int)pid);
		if (free_mem) {
			// FALTA TRATAR MORTE DO PROCESSO FILHO
			//     LIMPANDO TODOS OS RECURSOS ALOCADOS AO SUBPROCESSO
			// ????
		}
		continue;
	}
	// Unblock SIGCHLD signal
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigprocmask(SIG_UNBLOCK, &set, &oldset);
	// Reinstalls handler for SIGCHLD signal
	signal(SIGCHLD, reaper);
#ifdef DEBUG
	fprintf(stderr, "reaper ended\n");
#endif  
}



// Callback that receives data from the pipes
gboolean callback_pipe(GIOChannel *chan, GIOCondition cond, gpointer ptr)
{
	static char write_buf[1024];
	static char buf[MAX_MESSAGE_LENGTH];// buffer for reading data
	struct in_addr ip;
	short unsigned int porto;
	int n;
	int pid= (int)ptr;// PID do processo que mandou dados
	int p= g_io_channel_unix_get_fd(chan); // Obtém descritor de ficheiro do pipe
	guint chan_id; // Número de canal

	if (cond == G_IO_IN)
	{
		/* Read data from the socket */
		n = read_data_ipv6(s, buf, MAX_MESSAGE_LENGTH, &ip, &porto);
		if (n <= 0)
		{
			Log ("Read from socket failed\n");
			return TRUE;
			// Keeps waiting for more data
		} else {  // n > 0
			time_t tbuf;
			short unsigned int m;
			char *pt;
			/* Writes date and sender of the packet */
			time (&tbuf);			// Gets current date
			sprintf (write_buf, "%sReceived %d bytes from %s:%hu\n",ctime (&tbuf), n, inet_ntoa (ip), porto);
			Log (write_buf);/* Read the message fields */
			pt = buf;
			READ_BUF (pt, &m, sizeof(m));// Reads short and moves pointer
			m = ntohs (m);// Converts the number to host format
			if (m != n - 2)
			{
				sprintf (write_buf, "Invalid 'length' field (%d != %d)\n", m, n - 2);
				Log (write_buf);
				return TRUE;// Keeps waiting for more data
			}
			/* Writes data to the memo box - assumes that it ends with '\0' */
			Log (pt);// pt points to the first byte of the string
			Log ("\n");
			return TRUE;// Keeps waiting for more data
		}
	}
	else if ((cond == G_IO_NVAL) || (cond == G_IO_ERR))
	{
		Log ("Detected socket error\n");
		remove_socket_from_mainloop (s, &chan_id, chan);
		chan = NULL;
		close_socket (s);
		s = -1;/* Stops the application */
		gtk_main_quit ();
		return FALSE;// Removes socket's callback from main cycle
	} else {
		assert (0);// Must never reach this line - aborts application with a core dump
		return FALSE;// Removes socket's callback from main cycle
	}
}






// Receives SIGUSR1 in a subprocess
void rcv_reaper(int sig) {
	g_print("%ssignal %d - aborting transmission\n", name_str, sig);

	// a COMPLETAR para tratar o signal - caso necessário :)

	_exit(0);	// Exit code is zero!
}


// Receives SIGURG in a subprocess
void URG_reaper(int sig) {
	// a COMPLETAR para tratar o signal - caso necessário :)

}


// Starts a subprocess for file reception
void start_rcv_child_process (int msgsock, const char *filename, gboolean optimal)
{
#define RCV_BUFLEN 8000
	// Starts a subprocess that receives data from the TCP socket

	int p[2];	// descritor de pipe
	int n;
	int sock_t;
	int len_nome;
	char *nome;
	int len_filename;
	long filelength;
	int success;
	struct sockaddr_in6 remote_addr;

	if (pipe(p))	// Só suporta comunicação    p[1] -> p[0]
		perror("falhou pipe");


	//*************************************************************************************
	//*      SUBPROCESS                                                                   *
	//*************************************************************************************
	// Code running in the subprocess just created

	n = fork();

	if (n < 0) {
		Log("fork failed");
		exit(-1);
	}
	else
		if (n == 0) {

			// Prepares struct sockaddr_in variable 'name', with destination data
			int sock_t = init_socket_ipv6(SOCK_STREAM, 0,FALSE);

			if (connect(sock_t, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) <0)
			{
				Log("ERROR: Failed to connect to the host!\n");
				return;
			}

			else Log("Connected\n");

			/*Receive file*/

			n= read(sock_t, &len_nome, 4);

			if (n < 0)
				Log("Reading on stream socket1\n");

			n= read(sock_t, nome, len_nome);

			if (n < 0)
				Log("Reading on stream socket2\n");

			n= read(sock_t, &len_filename, 4);

			if (n < 0)
				Log("Reading on stream socket3\n");

			n = read(sock_t, filename, len_filename);

			if (n < 0)
				Log("Reading on stream socket4\n");

			n = read(sock_t, &filelength, 4);

			if (n < 0)
				Log("Reading on stream socket5\n");

			len_nome = strlen(nome)+1;
			len_filename = strlen(filename)+1;
			filelength = get_filesize(filename);

			char rcvbuf[filelength]; // Send buffer

			char* f_name = "file000.out";
			FILE *f2 = fopen(f_name, "a");

			if(f2 == NULL){
				Log("Error opening new file\n");
				return;

			}
			else
			{
				int s;
				while( s = recv(sock_t,rcvbuf,filelength, 0)){
					if(s<0){
						Log("ERROR: Failed to send file\n");
						break;
					}


					int write_sz = fwrite(rcvbuf, sizeof(char), s, f2);
					if(write_sz < s)
					{
						Log("File write failed.\n");
						break;
					}


				}

				fclose(f2);
				close(sock_t);
			}
		}
		else{

			//*************************************************************************************
			//*      end of subprocess code                                                        *
			//*************************************************************************************
			//del_in_process_list(n,  );
			GUI_remove_subprocess(n);
		}

	// Adds to the FList table

}




// Starts subprocess for sending a file
void start_file_upload_subprocess(struct in6_addr *ip_file, u_short porto,
		const char *nome, const char *filename, gboolean optimal)
{

	int p[2];	// descritor de pipe
	int n;
	int sock_t;
	int len_nome;
	int len_filename;
	long filelength;
	int success;
	struct sockaddr_in6 remote_addr;


	signal(SIGCHLD, reaper);	// arma callback para sinal SIGCHLD
	if (pipe(p))	// Só suporta comunicação    p[1] -> p[0]
		perror("falhou pipe");

	n = fork();

	if (n < 0) {
		Log("fork failed");
		exit(-1);
	}
	else
		if (n == 0) {
			/***************************************************************************/
			// Código do processo filho

			FILE *f = fopen(filename, "r");
			if (f == NULL) {
				Log("Select a valid file to transmit and try again\n");
				// Open window
				on_buttonFilename_clicked(NULL, NULL);
				return;
			}

			else

			// Prepares struct sockaddr_in variable 'name', with destination data
			sock_t = init_socket_ipv6(SOCK_STREAM, 0,FALSE);
			remote_addr.sin6_family = AF_INET6; // define IPv6
			remote_addr.sin6_flowinfo = 0;
			remote_addr.sin6_port = htons((short)porto); // define Port
			bcopy(&ip_file, &remote_addr.sin6_addr, sizeof(ip_file)); // define IP


			if (connect(sock_t, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) <0)
			{
				Log("ERROR: Failed to connect to the host!\n");
				return;
			}

			else Log("Connected\n");

			/*Send file*/
			len_nome = strlen(nome)+1;
			len_filename = strlen(filename)+1;
			filelength = get_filesize(filename);

			n= write(sock_t, &len_nome, 4);

			if (n < 0)
				Log("writing on stream socket1\n");

			n= write(sock_t, nome, len_nome);

			if (n < 0)
				Log("writing on stream socket2\n");

			n= write(sock_t, &len_filename, 4);

			if (n < 0)
				Log("writing on stream socket3\n");

			n = write(sock_t, filename, len_filename);

			if (n < 0)
				Log("writing on stream socket4\n");

			n = write(sock_t, &filelength, 4);

			if (n < 0)
				Log("writing on stream socket5\n");

			char sdbuf[filelength]; // Send buffer

			int s;
			do{
				s = fread(sdbuf, sizeof(char), filelength, f);
				if(s > 0){
					if(send(sock_t, sdbuf, s, 0) < 0)
					{
						Log("ERROR: Failed to send file\n");
						break;
					}

				}
				else {
					Log("Finished sending!\n");
				}
			}
			while(s>0);


			fclose(f);
			close(sock_t);

			_exit(0);
			// Termina processo filho
			/***************************************************************************/
		}
		else {
			// Código do processo pai
			close(p[1]); // Pai usa p[0]

			//fprintf (stderr, "Subprocess %d ended - sent %d/%d bytes\n", n, m, filelength);

			put_in_process_list(n, 0,&filename, "SND");
			GUI_regist_subprocess(n, 0, 'tipo' ,&nome, filename);
		}
}


