/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2015/2016
 *
 * callbacks.c
 *
 * Functions that handle main application logic for UDP communication, controlling query forwarding
 *
 * Updated on September 25, 19:00
 * @author  Luis Bernardo, Rodolfo Oliveira
\*****************************************************************************/
#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <assert.h>
#include <time.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include "sock.h"
#include "file.h"
#include "gui.h"
#include "subprocess.h"
#include "callbacks.h"
#include <netdb.h>
#include <sys/wait.h>
#include <string.h>

#ifdef DEBUG
#define debugstr(x)     g_print(x)
#else
#define debugstr(x)
#endif

/**********************\
|*  Global variables  *|
\**********************/

gboolean active = FALSE; 	// TRUE if server is active
char *user_name = NULL; // User name


gboolean active4 = FALSE; // TRUE if IPv4 is on and IPv6 if off
gboolean active6 = FALSE; // TRUE if IPv6 is on and IPv4 is off

guint query_timer_id = 0; // Timer event

gboolean changing = FALSE; // If it is changing the network

GList *tcp_conn = NULL; // List with active TCP connections/subprocesses


/******************************\
|*  Network Global variables  *|
\******************************/
u_short port_MCast = 0; // IPv4/IPv6 Multicast port

int sockUDP4 = -1; // IPv4 UDP socket descriptor
const char *str_addr_MCast4 = NULL;// IPv4 multicast address
struct sockaddr_in addr_MCast4; // struct with data of IPv4 socket
struct ip_mreq imr_MCast4; // struct with IPv4 multicast data

int sockUDP6 = -1; // IPv6 UDP socket descriptor
const char *str_addr_MCast6 = NULL;// IPv6 multicast address
struct sockaddr_in6 addr_MCast6; // struct with data of IPv6 socket
struct ipv6_mreq imr_MCast6; // struct with IPv6 multicast data

GIOChannel *chanUDP = NULL; // GIO channel descriptor of socket UDPv4 or UDPv6
guint chanUDP_id = 0; // channel number of socket UDPv4 or UDPv6

guint nome_timer_id = 0; // Timer event id

u_short port_TCP = 0; // TCP port
int sockTCP = -1; // IPv6 TCP socket descriptor
GIOChannel *chanTCP = NULL; // GIO channel descriptor of TCPv6 socket
guint chanTCP_id = 0; // Channel number of socket TCPv6
// Maximum message length
#define MAX_MESSAGE_LENGTH 5000
/*********************\
|*  Local variables  *|
 \*********************/
// Used to define unique numbers for incoming files
static int counter = 0;
// Temporary buffer
static char tmp_buf[8000];
static char tmp_buf2[8000];



/****************************************\
|* Functions to handle list of users    *|
 \****************************************/




// Handle REGISTRATION/CANCELLATION packets
gboolean process_registration(const char *name, int n, const char *ip_str,
		u_short port, gboolean registration) {

	if(registration==TRUE){

		GUI_regist_name(name, ip_str, port);
	}else{

		GUI_cancel_name(name, ip_str,port);
	}


	return TRUE;
}


// Sends a message using the IPv6 multicast socket
gboolean send_multicast(const char *buf, int n) {
	if (!active) {
		debugstr("active is false in send_multicast\n");
		return FALSE;
	}
	assert(active4 ^ active6);
	assert((sockUDP4>0) ^ (sockUDP6>0));
	// Sends message.
	if (active4) {
		if (sendto(sockUDP4, buf, n, 0, (struct sockaddr *) &addr_MCast4,
				sizeof(addr_MCast4)) < 0) {

			perror("Error while sending multicast datagram");
			return FALSE;
		}
	} else {
		if (sendto(sockUDP6, buf, n, 0, (struct sockaddr *) &addr_MCast6,
				sizeof(addr_MCast6)) < 0) {
			perror("Error while sending multicast datagram");
			return FALSE;
		}
	}
	return TRUE;
}


// Create a REGISTRATION/CANCELLATION message with the name and sends it
void multicast_name(gboolean registration) {
	unsigned char cod;
	char *pt = tmp_buf;

	assert(user_name != NULL);
	assert(port_TCP>0);
	cod = (registration ? REGISTRATION_NAME : CANCELLATION_NAME);
	WRITE_BUF(pt, &cod, 1);
	WRITE_BUF(pt, &port_TCP, 2);
	WRITE_BUF(pt, user_name, strlen(user_name)+1);

	send_multicast(tmp_buf, pt - tmp_buf);
}






// Timer callback for periodical registration of the server's name
gboolean callback_name_timer(gpointer data) {

	GtkTreeIter iter;
		GtkTreeModel *list_store = GTK_TREE_MODEL(main_window->listUsers);
		gboolean valid;

		const char *name;
		const char *ip_str;
		u_short port;

		valid = gtk_tree_model_get_iter_first(list_store, &iter);

		while (valid) {
			if (!GUI_test_name_timer(&iter)) {
				gtk_tree_model_get(list_store, &iter, 0, &ip_str, 1, &port, 2,
						&name, -1);
				GUI_cancel_name(name, ip_str, port);
			}
			valid = gtk_tree_model_iter_next(list_store, &iter);
		}
		multicast_name(TRUE);
		return TRUE;
}

// Callback to receive data from UDP socket
gboolean callback_UDP_data(GIOChannel *source, GIOCondition condition,
		gpointer data) {
	static char buf[MESSAGE_MAX_LENGTH]; // buffer for reading data
	struct in6_addr ipv6;
	struct in_addr ipv4;
	char ip_str[81];
	u_short port;
	int n;

	assert(main_window != NULL);
	if (!active) {
		debugstr("callback_UDP_data with active FALSE\n");
		return FALSE;
	}
	if (condition == G_IO_IN) {
		// Receive packet //


		if (active6) {
			n = read_data_ipv6(sockUDP6, buf, MESSAGE_MAX_LENGTH, &ipv6, &port);
			strncpy(ip_str, addr_ipv6(&ipv6), sizeof(ip_str));
		} else if (active4) {
			n = read_data_ipv4(sockUDP4, buf, MESSAGE_MAX_LENGTH, &ipv4, &port);
			strncpy(ip_str, addr_ipv4(&ipv4), sizeof(ip_str));
		} else {
			if (changing)
				return TRUE;
			assert(active6 || active4);
		}
		if (n <= 0) {
			Log("Failed reading packet from multicast socket\n");
			return TRUE; // Continue waiting for more events
		} else {
			time_t tbuf;
			unsigned char m;
			char *pt;
			struct timeval timeout;
			timeout.tv_sec =1;
			timeout.tv_usec=0;

			// Read data //
			pt = buf;
			READ_BUF(pt, &m, 1); // Reads type and advances pointer
			READ_BUF(pt, &port, 2); // Reads port and advances pointer
			// Writes date and sender's data //
			time(&tbuf);
			sprintf(tmp_buf, "%sReceived %d bytes from %s#%hu - type %hhd\n",
					ctime(&tbuf), n, ip_str, port, m);
			g_print("%s", tmp_buf);


			char nome[strlen(buf)-2];
			READ_BUF(pt, &nome, strlen(buf)-2);
			if(m==20)

				process_registration(nome,n,ip_str,port,FALSE);

			if(m==21)
				//	canc_timer_id = g_timeout_add(20000, GUI_cancel_name(, const char *ip, int port),user_name);
				process_registration(nome,n,ip_str,port,TRUE);


			return TRUE; // Keeps receiving more packets
		}
	} else if ((condition == G_IO_NVAL) || (condition == G_IO_ERR)) {
		Log("Error detected in UDP socket\n");
		// Turns sockets off
		close_all();
		// Closes the application
		gtk_main_quit();
		return FALSE; // Stops callback for receiving packets from socket
	} else {
		assert(0); // Should never reach this line
		return FALSE; // Stops callback for receiving packets from socket
	}
}



/************************************************************\
 |* Functions to handle the list of file transfer processes  *|
 \************************************************************/

// Add subprocess information to process list

Dados *put_in_process_list(int pid, int pipe, const char *filename,
		gboolean sending) {
	Dados *pt;
	//  g_print("new_desc(pid=%d ; pipe=%d ; %s)\n", pid, pipe, sending?"SND":"RCV");
	pt = (Dados *) malloc(sizeof(Dados));
	pt->pid = pid;
	pt->pipe = pipe;
	strcpy(pt->fname, filename);
	pt->sending = sending;
	if (!put_socket_in_mainloop(pt->pipe, pt, &pt->chan_id, &pt->chan, G_IO_IN, callback_pipe)) {
		fprintf(stderr, "failed registration of pipe in Gtk+ loop\n");
		free(pt);
		return NULL;
	}
	tcp_conn = g_list_append(tcp_conn, pt);
	return pt;
}


// Locate descriptor in subprocess list
Dados *locate_process_list(int pid) {
	GList *list = tcp_conn;
	while (list != NULL) {
		if (((Dados *) list->data)->pid == pid)
			return (Dados *) list;
		list = list->next;
	}
	return NULL;
}


// Delete descriptor in subprocess list
gboolean del_in_process_list(int pid, Dados *pt) {
	if (pt == NULL)
		pt = locate_process_list(pid);
	if (pt != NULL) {
		tcp_conn = g_list_remove(tcp_conn, pt);
	}
	return FALSE;
}




// Callback to receive connections at TCP socket
gboolean callback_connections_TCP(GIOChannel *source, GIOCondition condition,
		gpointer data) {
	}

// Stop the transmission of all files
void stop_all_files() {


	GtkTreeModel *list_store = GTK_TREE_MODEL(main_window->listFiles);
	GtkTreeIter iter;
	gboolean valid;

	// Get the first iter in the list
	valid = gtk_tree_model_get_iter_first(list_store, &iter);

	while (valid) {
		// Walk through the list, reading each row
		int pid;

		// Make sure you terminate calls to gtk_tree_model_get() with a '-1' value
		gtk_tree_model_get(list_store, &iter, 0, &pid, -1);




		// Send kill!
		// FALTA ENVIAR O KILL A TODOS OS PIDs
		send_kill(pid, SIGUSR1, FALSE);

		valid = gtk_tree_model_iter_next(list_store, &iter);
	}
}




// Start sending a file to the selected user - handle button "SendFile"
void on_buttonSendFile_clicked(GtkButton *button, gpointer user_data) {
	static char ip_str[81]; //remote ipv6
	char *ip, *name; // remote name and ipv4
	int n;
	int port; //remote port
	struct in6_addr ip_file;
	struct sockaddr_in6 remote_addr;
	int sock_t;
	int len_nome;
	int len_filename;
	long filelength;
	int success;
	int pid;
	GtkTreeIter iter;

	if (!active) {
		Log("This program is not active\n");
		return;
	}
	if (!GUI_get_selected_User(&ip, &port, &name, &iter)) {
		Log("No user is selected\n");
		return;
	}
	if (strchr(ip, ':') == NULL)
		// Converts address IPv4 to equivalent IPv6 address
		sprintf(ip_str, "::ffff:%s", ip);
	else
		strcpy(ip_str, ip);
	if (inet_pton(AF_INET6, ip_str, &ip_file) <= 0) {
		Log("Invalid IPv6 address found in the selected line\n");
		return;
	}


	//send_multicast(tmp_buf2, buffer - tmp_buf2);
	// Start sending the file
	// nomel tem o nome do utilizador remoto
	// ipl tem o endereço IP de destino (pode ser IPv4 ou IPv6)
	// porto tem o número de porto
	// filename tem o nome completo do ficheiro a enviar



	const char *filename = gtk_entry_get_text(main_window->FileName);
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
			remote_addr.sin6_port = htons((short)port); // define Port
			bcopy(&ip_file, &remote_addr.sin6_addr, sizeof(ip_file)); // define IP


		if (connect(sock_t, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) <0)
		{
			Log("ERROR: Failed to connect to the host!\n");
			return;
		}

		else Log("Connected\n");

		/*Send file*/
		len_nome = strlen(name)+1;
		len_filename = strlen(filename)+1;
		filelength = get_filesize(filename);

		n= write(sock_t, &len_nome, 4);

		if (n < 0)
			Log("writing on stream socket1\n");

		n= write(sock_t, name, len_nome);

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
		//send(sockUDP6,buffer,strlen(buffer),1);

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

	 
		start_file_upload_subprocess(&ip_file, port, name, filename, get_optimal());


}




// Stop the selected file transmission - handle button "Stop"
void on_buttonStop_clicked(GtkButton *button, gpointer user_data) {
	GtkTreeIter iter;
	int pid;

	if (!GUI_get_selected_Filetx(&pid, &iter)) {
		Log("No file transfer is selected\n");
		return;
	}
	// Send kill!
	send_kill(pid, SIGUSR1, FALSE);
}


/*********************************\
|* Functions to control sockets  *|
\*********************************/


// Close the UDP sockets
void close_sockUDP(void) {
	gboolean old_changing = changing;
	debugstr("close_sockUDP\n");
	// IPv4
	close(sockUDP4);
	sockUDP4 = -1;
	str_addr_MCast4 = NULL;
	active4 = FALSE;

	// IPv6
	close(sockUDP6);
	sockUDP6 = -1;
	str_addr_MCast6 = NULL;
	active6 = FALSE;

	changing = old_changing;
}


// Close TCP socket
void close_sockTCP(void) {


		close(sockTCP);
		sockTCP = -1;
	}


// Create IPv4 UDP socket, configure it, and register its callback
// It receives configurations from global variables:
//     port_MCast - multicast port
gboolean init_socket_udp4(const char *addr_multicast) {
	gboolean old_changing = changing;
	char loop = 1;

	if (active4)
		return TRUE;

	changing = TRUE;
	if ((sockUDP6 > 0) || (sockUDP4 > 0)) {
		debugstr("WARNING: 'init_sockets_udp4' closed UDP socket\n");
		close_sockUDP();
	}

	// Prepares the data structures
	if (!get_IPv4(addr_multicast, &addr_MCast4.sin_addr)) {
		return FALSE;
	}
	addr_MCast4.sin_port = htons(port_MCast);
	addr_MCast4.sin_family = AF_INET;
	bcopy(&addr_MCast4.sin_addr, &imr_MCast4.imr_multiaddr,
			sizeof(addr_MCast4.sin_addr));
	imr_MCast4.imr_interface.s_addr = htonl(INADDR_ANY);

	// Creates the IPV4 UDP socket
	sockUDP4 = init_socket_ipv4(SOCK_DGRAM, port_MCast, TRUE);
	fprintf(stderr, "UDP4 = %d\n", sockUDP4);
	if (sockUDP4 < 0) {
		Log("Failed opening IPv4 UDP socket\n");
		return FALSE;
	}
	// Join the group
	if (setsockopt(sockUDP4, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			(char *) &imr_MCast4, sizeof(struct ip_mreq)) == -1) {
		perror("Failed association to IPv4 multicast group");
		Log("Failed association to IPv4 multicast group\n");
		return FALSE;
	}
	str_addr_MCast4 = addr_multicast; // Memorizes it is associated to a group

	// Configures the socket to receive an echo of the multicast packets sent by this application
	setsockopt(sockUDP4, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

	// Regists the socket in the main loop of Gtk+
	if (!put_socket_in_mainloop(sockUDP4, (void *) 0, &chanUDP_id, &chanUDP, G_IO_IN,
			callback_UDP_data)) {
		Log("Failed registration of UDPv4 socket at Gnome\n");
		close_sockUDP();
		return FALSE;
	}
	active4 = TRUE;
	changing = old_changing;
	return TRUE;
}


// Create IPv6 UDP socket, configure it, and register its callback
// It receives configurations from global variables:
//     port_MCast - multicast port
gboolean init_socket_udp6(const char *addr_multicast) {


	gboolean old_changing = changing;
	char loop = 1;

	if (active6)
		return TRUE;

	changing = TRUE;
	if ((sockUDP6 > 0) || (sockUDP4 > 0)) {
		debugstr("WARNING: 'init_sockets_udp6' closed UDP socket\n");
		close_sockUDP();
	}

	// Prepares the data structures
	if (!get_IPv6(addr_multicast, &addr_MCast6.sin6_addr)) {
		return FALSE;
	}
	addr_MCast6.sin6_port = htons(port_MCast);
	addr_MCast6.sin6_family = AF_INET6;
	bcopy(&addr_MCast6.sin6_addr, &imr_MCast6.ipv6mr_multiaddr,
			sizeof(addr_MCast6.sin6_addr));
	imr_MCast6.ipv6mr_interface = htonl(INADDR_ANY);

	// Creates the IPV4 UDP socket
	sockUDP6 = init_socket_ipv6(SOCK_DGRAM, port_MCast, TRUE);
	fprintf(stderr, "UDP6 = %d\n", sockUDP6);


	if (sockUDP6 < 0) {
		Log("Failed opening IPv6 UDP socket\n");
		return FALSE;
	}
	// Join the group
	if (setsockopt(sockUDP6, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
			(char *) &imr_MCast6, sizeof(struct ipv6_mreq)) == -1) {


		perror("Failed association to IPv6 multicast group");
		Log("Failed association to IPv6 multicast group\n");
		return FALSE;
	}
	str_addr_MCast6 = addr_multicast; // Memorizes it is associated to a group

	// Configures the socket to receive an echo of the multicast packets sent by this application
	setsockopt(sockUDP6, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop));

	// Regists the socket in the main loop of Gtk+
	if (!put_socket_in_mainloop(sockUDP6, (void *) 0, &chanUDP_id, &chanUDP, G_IO_IN,
			callback_UDP_data)) {
		Log("Failed registration of UDPv6 socket at Gnome\n");
		close_sockUDP();
		return FALSE;
	}


	active6 = TRUE;
	changing = old_changing;
	return TRUE;
}


// Initialize all sockets. Receives configuration from global variables:
// str_addr_MCast4/6 - IP multicast address
// imr_MCast4/6 - struct for association to to IP Multicast address
// port_MCast4/6 - multicast port
// addr_MCast4/6 - struct with UDP socket data for sending packets to the group
gboolean init_sockets(gboolean is_ipv6, const char *addr_multicast) {
	int sock;

	if (is_ipv6) {
		
		if (!init_socket_udp6(addr_multicast)){

			return FALSE;
		}
		//TCP
		if((sock= init_socket_ipv6(SOCK_STREAM,0,FALSE))==-1){

			return FALSE;
		}

		if(!put_socket_in_mainloop(sock,main_window,&chanTCP_id,&chanTCP,G_IO_IN,callback_connections_TCP)){

			return FALSE;
		}

		listen(sock,5);
		port_TCP=get_portnumber(sock);
		set_portT_number(port_TCP);

	} else {
		if (!init_socket_udp4(addr_multicast))
			return FALSE;
		//Socket TCP
		if((sock= init_socket_ipv4(SOCK_STREAM,0,FALSE))==-1)
			return FALSE;
		listen(sock,5); //nthos();
		if(!put_socket_in_mainloop(sock,main_window,&chanTCP_id,&chanTCP,G_IO_IN,callback_connections_TCP)){
			return FALSE;
		}


		//port_TCP=ntohs(in6 addr_MCast6);
		port_TCP=get_portnumber(sock);
		set_portT_number(port_TCP);
	}


	return TRUE;
}


/*******************************************************\
|* Functions to control the state of the application   *|
\*******************************************************/


// Closes everything
void close_all(void) {
	gboolean old_changing = changing;
	changing = TRUE;
	if (nome_timer_id > 0)
		g_source_remove(nome_timer_id);
	if (user_name != NULL)
		multicast_name(FALSE);
	close_sockUDP();
	close_sockTCP();
	set_portT_number(0);
	stop_all_files();
	if (user_name != NULL) {
		free(user_name);
		user_name = NULL;
	}

	GUI_clear_names();
	changing = old_changing;
}


// Button that starts and stops the application
void on_togglebuttonActive_toggled(GtkToggleButton *togglebutton, gpointer user_data) {

	if (gtk_toggle_button_get_active(main_window->active)) {

		// *** Starts the server ***
		const gchar *addr_str, *textNome;
		gboolean is_ipv6;
		gboolean b6 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(main_window->check_ip6));
		gboolean b4 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(main_window->check_ip4));
		assert(b4 ^ b6);
		changing = TRUE;

		// Get local IP
		set_local_IP();
		gtk_entry_set_text(main_window->entryIPv6loc, addr_ipv6(&local_ipv6));
		gtk_entry_set_text(main_window->entryIPv4loc, addr_ipv4(&local_ipv4));
		// Read parameters
		if ((textNome = gtk_entry_get_text(main_window->entryName)) == NULL) {
			Log("Undefined user name\n");
			gtk_toggle_button_set_active(togglebutton, FALSE); // Turns button off
			changing = FALSE;
			return;
		}
		int n = get_portM_number();
		if (n < 0) {
			Log("Invalid multicast port number\n");
			gtk_toggle_button_set_active(togglebutton, FALSE); // Turns button off
			changing = FALSE;
			return;
		}
		port_MCast = (unsigned short) n;
		if (!(addr_str = get_IPmult(&is_ipv6, &addr_MCast4.sin_addr,
				&addr_MCast6.sin6_addr))) {
			Log("Invalid IP multicast address\n");
			gtk_toggle_button_set_active(togglebutton, FALSE); // Turns button off
			changing = FALSE;
			return;
		}
		if (b6 != is_ipv6) {
			Log("Invalid IP version of multicast address\n");
			gtk_toggle_button_set_active(togglebutton, FALSE); // Turns button off
			changing = FALSE;
			return;
		}
		if (b6) {
			addr_MCast6.sin6_port = htons(port_MCast);
			addr_MCast6.sin6_family = AF_INET6;
			addr_MCast6.sin6_flowinfo = 0;
			bcopy(&addr_MCast6.sin6_addr, &imr_MCast6.ipv6mr_multiaddr,
					sizeof(addr_MCast6.sin6_addr));
			imr_MCast6.ipv6mr_interface = 0;
		} else {
			assert(b4); // IPv4
			addr_MCast4.sin_port = htons(port_MCast);
			addr_MCast4.sin_family = AF_INET;
			bcopy(&addr_MCast4.sin_addr, &imr_MCast4.imr_multiaddr,
					sizeof(addr_MCast4.sin_addr));
			imr_MCast4.imr_interface.s_addr = htonl(INADDR_ANY);
		}
		if (!init_sockets(b6, addr_str)) {
			Log("Failed configuration of server\n");
			gtk_toggle_button_set_active(togglebutton, FALSE); // Turns button off
			changing = FALSE;
			return;
		}
		set_portT_number(port_TCP);

		// ****
		// Starts periodical sending of the NAME
		// Inicia envio periodico de NOME

		user_name = strdup(textNome);
		nome_timer_id = g_timeout_add(NAME_TIMER_PERIOD, callback_name_timer,
				user_name);



		// ****

		block_entrys(FALSE);
		changing = FALSE;
		active = TRUE;

		// Sends the local name
		// multicast_name(TRUE);
		Log("fileexchange active\n");

	} else {

		// *** Stops the server ***
		close_all();
		block_entrys(TRUE);
		active = FALSE;
		Log("fileexchange stopped\n");
	}

}


// IPv4 type button modified; it redefines the multicast address and the socket type
void on_checkbuttonIPv4_toggled(GtkToggleButton *togglebutton,
		gpointer user_data) {
	changing = TRUE;
	// Set IP to IPv4 multicast address
	gboolean b4 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(main_window->check_ip4));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(main_window->check_ip6), !b4);
	gtk_entry_set_text(main_window->entryMIP, g_strdup(!b4 ? "ff18:10:33::1"
			: "225.0.0.1"));
	if (active) {
		if ((!b4 ? !init_socket_udp6("ff18:10:33::1") : !init_socket_udp4(
				"225.0.0.1"))) {
			// Creation of socket failed
			close_all();
			gtk_toggle_button_set_active(main_window->active, FALSE);
		}
	}
	changing = FALSE;
	if (active) {
		// Sends its local name
		//multicast_name(TRUE);
	}
}


// IPv6 type button modified; it redefines the multicast address and the socket type
void on_checkbuttonIPv6_toggled(GtkToggleButton *togglebutton,
		gpointer user_data) {
	changing = TRUE;
	// Set IP to IPv6 multicast address
	gboolean b6 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(main_window->check_ip6));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(main_window->check_ip4), !b6);
	gtk_entry_set_text(main_window->entryMIP, g_strdup(b6 ? "ff18:10:33::1"
			: "225.0.0.1"));
	if (active) {
		if ((b6 ? !init_socket_udp6("ff18:10:33::1") : !init_socket_udp4(
				"225.0.0.1"))) {
			// Creation of socket failed
			close_all();
			gtk_toggle_button_set_active(main_window->active, FALSE);
		}
	}
	changing = FALSE;
	if (active) {
		// Sends its local name
		//multicast_name(TRUE);
	}
}


// The user closed the main window
gboolean on_window1_delete_event(GtkWidget *widget, GdkEvent *event,
		gpointer user_data) {
	close_all();
	gtk_main_quit();
	return FALSE;
}






/*
/ **************************************************\
|* Functions that handle TCP socket communication *|
\************************************************** /
 */
