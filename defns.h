/*
 * defns.h
 *
 *  Created on: Sep 14, 2013
 *      Author: Harish Mangalampalli
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include<limits.h>


#ifndef DEFNS_H_
#define DEFNS_H_

#define MAXDATASIZE 500
#define INFINITY SHRT_MAX
#define INFINITYSTRING "INFINITY"
#define HELPUSAGE "Usage: server -t <topology-file-name> -i <routing-update-interval>\n"
#define HELPCOMMANDS "Commands:\n"
#define HELPHELP "\tHELP\tDisplay information about the available user interface options.\n"
#define HELPMYIP "\tMYIP\tDisplay the IP address of this process.\n"
#define HELPMYPORT "\tMYPORT\tDisplay the port on which this process is listening for incoming connections.\n"
#define HELPREGISTER "\tREGISTER <server IP> <port_no>\tThis command is used by the client to register itself with the server and to get the IP and listening port numbers of all the peers currently registered with the server.\n\tNote: The REGISTER command works only on the client and does not work on the server.\n"
#define HELPCONNECT "\tCONNECT <destination> <port no>\tThis command establishes a new TCP connection to the specified <destination> at the specified < port no>. The <destination> can either be an IP address or a hostname.\n"
#define HELPLIST "\tLIST\tDisplay a numbered list of all the connections this process is part of. This numbered list will include connections initiated by this process and connections initiated by other processes.\n"
#define HELPTERMINATE "\tTERMINATE <connection id>\tThis command will terminate the connection listed under the specified number when LIST is used to display all connections.\n"
#define HELPEXIT "\tEXIT\tClose all connections and terminate this process.\n"
#define HELPDOWNLOAD "\tDOWNLOAD <file_name> <file_chunk_size_in_bytes>\tThe host shall download in parallel different chunks of file specified in the download command from all connected hosts until the complete file is downloaded. Please note that the total number of hosts that the client can download from can be 1, 2, 3 or 4. The hosts will be part of the list of hosts displayed by the LIST command.\n"
#define HELPCREATOR "\tCREATOR\tDisplay the creator's full name, UBIT name and UB email address.\n"
#define CREATOR "Full name: HARISH MANGALAMPALLI\nUBIT name: hmangala\nUB email address: hmangala@buffalo.edu\n"
#define HOSTNAMESTRING "Hostname"
#define IPSTRING "IP address"
#define PORTSTRING "Port No."
#define NUM_CHUNKS_STRING "Number of Chunks Downloaded"
#define HELPUPDATE "Usage: update <server-ID1> <server-ID2> <Link Cost>\n"
#define HELPDISABLE "Usage: disable <server-ID>\n"

struct Dist_Vec {
	int server_id;
	int neighbor_id;
	int next_hop;
	int cost;
	int sockfd;
	int backlog;
	int disabled;
	int unresponsive;
	struct Dist_Vec *next;
};

struct Neigh_Dist_Vec {
	int neighbor_id;
	struct Dist_Vec *dist_vec_list;
	struct Neigh_Dist_Vec *next;
};

struct ServerIP {
	int server_id;
	char* ip;
	int port;
	struct ServerIP *next;
};

struct Topology {
	int num_servers;
	int num_neighbors;
	int numPackets;
	struct ServerIP *server_list;
	struct Dist_Vec *dist_vec_list;
};

#endif /* DEFNS_H_ */
