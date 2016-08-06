/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2015/2016
 *
 * callbacks.h
 *
 * Header file of functions that handle main application logic for UDP communication,
 *    controlling query forwarding
 *
 * Updated on September 25, 19:00
 * @author  Luis Bernardo, Rodolfo Oliveira
\*****************************************************************************/

#include <gtk/gtk.h>
#include <glib.h>
#include <netinet/in.h>
#include "gui.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif


// Directory pathname to write output files
extern char *out_dir;
// User name
extern char *user_name;
// List with active TCP connections/subprocesses
extern GList *tcp_conn;


#ifndef CALLBACKS_INC_
#define CALLBACK_INC_

//#define DEBUG
#define MESSAGE_MAX_LENGTH	9000

/* Packet types */
#define REGISTRATION_NAME		21
#define CANCELLATION_NAME		20

/* Clock period durations */
#define NAME_TIMER_PERIOD	10000


// TCP connection information
typedef struct Dados {
    gboolean sending;
    char fname[256];	// if (!sending) has the filename being recorded
    int len;		// if (!sending) has the block size being received
    int pid;		// pid of the subprocess
    int pipe;		// pipe of the connection to the subprocess
    guint chan_id;	// GIO channel
    GIOChannel *chan;	// GIO channel object with buffering
    gboolean finished;	// If it finished the transference
    gboolean ok;	// If it ended with success
} Dados ;


/****************************************\
|* Functions to handle list of users    *|
 \****************************************/
// Handle REGISTRATION/CANCELLATION packets
gboolean process_registration(const char *name, int n, const char *ip_str,
		u_short port, gboolean registration);
// Sends a message using the IPv6 multicast socket
gboolean send_multicast(const char *buf, int n);
// Create a REGISTRATION/CANCELLATION message with the name and sends it
void multicast_name(gboolean registration);
// Test the timer for all neighbors
void test_all_name_timer(void);
// Timer callback for periodical registration of the server's name
gboolean callback_name_timer(gpointer data);
// Callback to receive data from UDP socket
gboolean callback_UDP_data(GIOChannel *source, GIOCondition condition,
		gpointer data);

/*************************************************************\
|* Functions to handle the list of file transfer processes   *|
 \************************************************************/
// Add subprocess information to process list
Dados *put_in_process_list(int pid, int pipe, const char *filename,
		gboolean sending);
// Locate descriptor in subprocess list
Dados *locate_process_list(int pid);
// Delete descriptor in subprocess list
gboolean del_in_process_list(int pid, Dados *pt);
// Stop the transmission of all files
void stop_all_files();
// Callback to receive connections at TCP socket
gboolean callback_connections_TCP(GIOChannel *source, GIOCondition condition,
		gpointer data);


// Start sending a file to the selected user - handle button "SendFile"
void
on_buttonSendFile_clicked                (GtkButton       *button,
        								 gpointer         user_data);
// Stop the selected file transmission - handle button "Stop"
void
on_buttonStop_clicked                 	(GtkButton       *button,
		                                 gpointer         user_data);

/*********************************\
|* Functions to control sockets  *|
\*********************************/
// Close the UDP sockets
void close_sockUDP(void);
// Close TCP socket
void close_sockTCP(void);
// Create IPv4 UDP socket, configure it, and register its callback
// It receives configurations from global variables:
//     port_MCast - multicast port
gboolean init_socket_udp4(const char *addr_multicast);
// Create IPv6 UDP socket, configure it, and register its callback
// It receives configurations from global variables:
//     port_MCast - multicast port
gboolean init_socket_udp6(const char *addr_multicast);
// Initialize all sockets. Receives configuration from global variables:
// str_addr_MCast4/6 - IP multicast address
// imr_MCast4/6 - struct for association to to IP Multicast address
// port_MCast4/6 - multicast port
// addr_MCast4/6 - struct with UDP socket data for sending packets to the group
gboolean init_sockets(gboolean is_ipv6, const char *addr_multicast);


/*******************************************************\
|* Functions to control the state of the application   *|
\*******************************************************/
// Closes everything
void close_all(void);
// Button that starts and stops the application
void
on_togglebuttonActive_toggled          (GtkToggleButton *togglebutton,
                                        gpointer         user_data);
// IPv4 type button modified; it redefines the multicast address and the socket type
void
on_checkbuttonIPv4_toggled             (GtkToggleButton *togglebutton,
                                        gpointer         user_data);
// IPv6 type button modified; it redefines the multicast address and the socket type
void
on_checkbuttonIPv6_toggled             (GtkToggleButton *togglebutton,
                                        gpointer         user_data);
// The user closed the main window
gboolean
on_window1_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);
#endif
