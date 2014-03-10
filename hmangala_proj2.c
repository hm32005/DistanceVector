/*
 * proj1.c
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
#include "defns.h"

void createServer(); // function to create a server that listens to updates and sends out own distance vectors to neighbors
struct Dist_Vec *copy(struct Dist_Vec *head); // function to copy a given node in a linked list
int update(int serverId, int neighborId, int cost, struct Dist_Vec *dist_vec_list); // function to update link costs
int step(struct Dist_Vec *dist_vec_list); // function to send out distance vector updates to neighbors
void display(struct Dist_Vec *dist_vec_list); // function to display current distance vector
void recomputeDV(struct Dist_Vec *dist_vec_list); // function to compute current distance vector from link costs and neighbors' distance vectors
void freeDVList(struct Dist_Vec* head); // function to free distance vector linked lists
void freeNeighborDVList(struct Neigh_Dist_Vec* head); // function to free the linked list holding neigbors' distance vectors
void freeTopList(struct Topology* head); // function to free the initially read topology
int disable(int neighbor_id); // function to disable link between a particular neighbor
void displayCost(); // function to display initial cost to neighbors
int listLength(struct Dist_Vec* item); // function to compute length of linked list holding distance vectors

char *top_file, serverIp[128];
long update_interval;
int serverPort, listener;
long int server_id;
struct Topology *serverTop; // structure to hold initially read topology
struct Neigh_Dist_Vec *neighborDVList = NULL; // structure to hold neigbors' distance vectors
struct Dist_Vec *dist_vec_list = NULL; // structre to hold current distance vectors

// main function
int main(int argc, char *argv[]) {
	if (argc == 5) {
		if (strcasecmp(argv[1], "-t") == 0 && strcasecmp(argv[3], "-i") == 0) {
			top_file = argv[2];
			update_interval = strtol(argv[4], NULL, 10);
			createServer();
		} else if (strcasecmp(argv[1], "-i") == 0 && strcasecmp(argv[3], "-t") == 0) {
			top_file = argv[4];
			update_interval = strtol(argv[2], NULL, 10);
			createServer();
		} else {
			fprintf(stderr, "\n%s\n", HELPUSAGE);
			exit(0);
		}
	} else {
		fprintf(stderr, "\n%s\n", HELPUSAGE);
		exit(0);
	}
	return 0;
}

// function to create a server that listens to updates and sends out own distance vectors to neighbors
void createServer() {
	int fdmax = -1, selectValue, i, j, yes = 1, nbytes;
	char *s;
	char line[500];
	char stdinbuf[MAXDATASIZE];
	struct timeval tv;

	fd_set master;
	fd_set read_fds;

	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	// add STDIN to fdset
	FD_SET(fileno(stdin), &master);
	if (fileno(stdin) > fdmax) {
		fdmax = fileno(stdin);
	}

	//read topology file and save in temp list
	FILE *in_file = fopen(top_file, "r");
	if (in_file == NULL) {
		perror("FILE IO: ");
		exit(1);
	} else {
		serverTop = (struct Topology *) malloc(sizeof(*serverTop));
		serverTop->numPackets = 0;
		struct ServerIP *server_list = NULL, *curServer, *newServer;
		struct Dist_Vec *init_dist_vec_list = NULL, *curDistVec, *newDistVec, *newNeighDistVec;
		struct Neigh_Dist_Vec *curNeighDV, *newNeighDV;
		for (i = 1; fgets(line, 500, in_file) != NULL; i++) {
			if (i == 1) {
				serverTop->num_servers = strtol(line, NULL, 10);
			} else if (i == 2) {
				serverTop->num_neighbors = strtol(line, NULL, 10);
			} else if (i >= 3 && i < 3 + serverTop->num_servers) {
				char* pch = strtok(line, " ");
				char** toks = NULL;
				int tokitr;
				toks = malloc(sizeof(char*) * 5);
				for (tokitr = 0; pch != NULL; tokitr++) {
					toks[tokitr] = pch;
					pch = strtok(NULL, " ");
				}
				// create node to hold server id, ip and port information
				newServer = (struct ServerIP *) malloc(sizeof(*server_list));
				newServer->ip = (char *) malloc(100 * sizeof(char));

				newServer->server_id = strtol(toks[0], NULL, 10);
				strcpy(newServer->ip, toks[1]);
				newServer->port = strtol(toks[2], NULL, 10);
				newServer->next = NULL;

				curServer = server_list;
				if (server_list != NULL) {
					while (curServer->next != NULL)
						curServer = curServer->next;
					curServer->next = newServer;
				} else {
					server_list = newServer;
				}
				free(toks);
			} else {
				char* pch = strtok(line, " ");
				char** toks = NULL;
				int tokitr;
				toks = malloc(sizeof(char*) * 5);
				for (tokitr = 0; pch != NULL; tokitr++) {
					toks[tokitr] = pch;
					pch = strtok(NULL, " ");
				}
				newDistVec = (struct Dist_Vec *) malloc(sizeof(*newDistVec));
				server_id = strtol(toks[0], NULL, 10);
				// create node to hold intial link cost and neighbor information
				newDistVec->server_id = strtol(toks[0], NULL, 10);
				newDistVec->neighbor_id = strtol(toks[1], NULL, 10);
				newDistVec->next_hop = strtol(toks[1], NULL, 10);
				newDistVec->cost = strtol(toks[2], NULL, 10);
				newDistVec->sockfd = 0;
				newDistVec->backlog = 0;
				newDistVec->disabled = 0;
				newDistVec->unresponsive = 0;
				newDistVec->next = NULL;

				// create node to hold neighbor's distance vector
				newNeighDistVec = (struct Dist_Vec *) malloc(sizeof(*newNeighDistVec));
				newNeighDistVec->server_id = strtol(toks[1], NULL, 10);
				newNeighDistVec->neighbor_id = strtol(toks[1], NULL, 10);
				newNeighDistVec->next_hop = strtol(toks[1], NULL, 10);
				newNeighDistVec->cost = 0;
				newNeighDistVec->sockfd = 0;
				newNeighDistVec->backlog = 0;
				newNeighDistVec->disabled = 0;
				newNeighDistVec->unresponsive = 0;
				newNeighDistVec->next = NULL;

				// create node to hold neighbors' id and linked list containing distance vectors
				newNeighDV = (struct Neigh_Dist_Vec *) malloc(sizeof(*newNeighDV));
				newNeighDV->dist_vec_list = newNeighDistVec;
				newNeighDV->neighbor_id = strtol(toks[1], NULL, 10);
				newNeighDV->next = NULL;

				if (neighborDVList != NULL) {
					curNeighDV = neighborDVList;
					while (curNeighDV->next != NULL)
						curNeighDV = curNeighDV->next;
					curNeighDV->next = newNeighDV;
				} else
					neighborDVList = newNeighDV;

				if (dist_vec_list != NULL) {
					curDistVec = dist_vec_list;
					while (curDistVec->next != NULL)
						curDistVec = curDistVec->next;
					curDistVec->next = copy(newDistVec);
				} else {
					// create node to hold link cost to self and insert at start of list containing link costs
					struct Dist_Vec *selfDistVec = (struct Dist_Vec *) malloc(sizeof(*selfDistVec));
					selfDistVec->server_id = server_id;
					selfDistVec->neighbor_id = server_id;
					selfDistVec->next_hop = server_id;
					selfDistVec->cost = 0;
					selfDistVec->disabled = 0;
					selfDistVec->unresponsive = 0;
					selfDistVec->backlog = 0;
					dist_vec_list = selfDistVec;
					dist_vec_list->next = copy(newDistVec); // copy link costs onto distance vector
				}

				if (init_dist_vec_list != NULL) {
					curDistVec = init_dist_vec_list;
					while (curDistVec->next != NULL)
						curDistVec = curDistVec->next;
					curDistVec->next = newDistVec;
				} else {
					init_dist_vec_list = newDistVec;
				}
				free(toks);
			}

		}
		fclose(in_file);
		serverTop->server_list = server_list;
		serverTop->dist_vec_list = init_dist_vec_list;

		// identify own ip and port from topology file
		struct ServerIP *cur = server_list;
		struct Dist_Vec *curDV = init_dist_vec_list;
		for (i = 1; i <= serverTop->num_servers; i++) {
			if (cur->server_id == curDV->server_id) {
				strcpy(serverIp, cur->ip);
				serverPort = cur->port;
				break;
			} else
				cur = cur->next;
		}
		// end of identification


		struct addrinfo hints, *ai, *p;
		int rv;
		char serverPortString[10];
		sprintf(serverPortString, "%d", serverPort);
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_PASSIVE;
		if ((rv = getaddrinfo(NULL, serverPortString, &hints, &ai)) != 0) {
			fprintf(stderr, "\nSERVER: %s\n", gai_strerror(rv));
			exit(1);
		}

		for (p = ai; p != NULL; p = p->ai_next) {
			listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (listener < 0) {
				continue;
			}

			setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
			// start listening for updates
			if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
				close(listener);
				continue;
			}
			break;
		}

		if (p == NULL) {
			fprintf(stderr, "\nFailed to bind to listening port number: %d\n", serverPort);
			exit(2);
		}

		freeaddrinfo(ai);

		// add listener socket to fd set
		FD_SET(listener, &master);
		if (listener > fdmax) {
			fdmax = listener;
		}
		// end of listener code

		// connect to neighbors defined in the topology file

		struct addrinfo sa_dest, *ai_dest;
		int neighbor_fd;
		char remotePortString[10];
		curDV = init_dist_vec_list;
		for (i = 1; i <= serverTop->num_neighbors; i++, curDV = curDV->next) {
			// identify client ip and port from topology file
			struct ServerIP *curNeighbor = server_list;
			int j;
			for (j = 1; j <= serverTop->num_servers; j++) {
				if (curNeighbor->server_id == curDV->neighbor_id) {
					break;
				} else
					curNeighbor = curNeighbor->next;
			}
			// end of identification
			memset(&sa_dest, 0, sizeof(struct addrinfo));
			sa_dest.ai_family = AF_INET;
			sa_dest.ai_socktype = SOCK_DGRAM;
			sprintf(remotePortString, "%d", curNeighbor->port);
			if ((rv = getaddrinfo(curNeighbor->ip, remotePortString, &sa_dest, &ai_dest)) != 0) {
				fprintf(stderr, "\nGETADDRINFO: %s\n", gai_strerror(rv));
				continue;
			}
			for (p = ai_dest; p != NULL; p = p->ai_next) {
				if (p->ai_family == AF_INET) {
					// create datagram socket to send distance vectors to neighbors
					if ((neighbor_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
						perror("\nSOCKET ");
						continue;
					}

					setsockopt(neighbor_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
					break;

					// connect to neighbors
					if ((connect(neighbor_fd, p->ai_addr, p->ai_addrlen)) == -1) {
						close(neighbor_fd);
						perror("\nCONNECT ");
						FD_CLR(neighbor_fd, &master);
						neighbor_fd = -1;
						continue;
					}
				}
			}
			if (p == NULL) {
				fprintf(stderr, "\nFailed to connect\n");
				close(neighbor_fd);
				FD_CLR(neighbor_fd, &master);
				neighbor_fd = -1;
				freeaddrinfo(ai_dest);
				continue;
			} else
				curDV->sockfd = neighbor_fd;
			FD_SET(neighbor_fd, &master);
			if (neighbor_fd > fdmax) {
				fdmax = neighbor_fd;
			}
			freeaddrinfo(ai_dest);
		}
		//end of connect to neighbors defined in the topology file
		displayCost();
		step(dist_vec_list);
	}

//start of select
	int breakOuterLoop = 0;
	tv.tv_sec = update_interval;
	tv.tv_usec = 0;
	int dndisp = 0, dndisp2 = 0;
	for (; breakOuterLoop != 1;) {
		if (dndisp == 0 && dndisp2 == 0) {
			printf("\nserver>>");
			fflush(stdout);
		}
		dndisp2 = 0;
		read_fds = master;
		selectValue = select(fdmax + 1, &read_fds, NULL, NULL, &tv);
		if (selectValue > 0) {
			for (i = 0; i <= fdmax; i++) {
				dndisp = 0;
				if (FD_ISSET(i, &read_fds)) {
					// received update on STDIN
					if (i == fileno(stdin)) {
						fgets(stdinbuf, MAXDATASIZE, stdin);
						int len = strlen(stdinbuf);
						if (stdinbuf[len - 1] == '\n')
							stdinbuf[len - 1] = '\0';
						s = stdinbuf;
						if (strncasecmp(stdinbuf, "update", strlen("update") - 1) == 0) {
							char* pch = strtok(stdinbuf, " ");
							char** toks = NULL;
							int tokitr, serverid, neighbor_id, link_cost;
							toks = malloc(sizeof(char*) * 5);
							for (tokitr = 0; pch != NULL; tokitr++) {
								toks[tokitr] = pch;
								pch = strtok(NULL, " ");
							}
							if (tokitr != 4) {
								fprintf(stderr, "\nUPDATE: Incorrect syntax\n");
								fprintf(stderr, "%s\n", HELPUPDATE);
							} else {
								serverid = strtol(toks[1], NULL, 10);
								neighbor_id = strtol(toks[2], NULL, 10);
								if (strcasecmp(toks[3], "inf") != 0)
									link_cost = strtol(toks[3], NULL, 10);
								else
									link_cost = INFINITY;

								//update link cost
								if (update(serverid, neighbor_id, link_cost, dist_vec_list) == 1)
									printf("\nUPDATE SUCCESS\n");
								else
									fprintf(stderr, "\nUPDATE: Server and/or neighbor id incorrect!\n");
							}
							free(toks);
						} else if (strcasecmp(s, "step") == 0) {
							if (step(dist_vec_list) == 1) {
								printf("\nSTEP SUCCESS\n");
								tv.tv_sec = update_interval;
								tv.tv_usec = 0;
							} else
								fprintf(stderr, "\nSTEP: Could not send routing update!\n");
						} else if (strcasecmp(s, "packets") == 0) {
							printf("\nNumber  of  distance  vector  packets received is %d\n", serverTop->numPackets);
							printf("\nPACKETS SUCCESS\n");
							serverTop->numPackets = 0; // reset packet count to zero
						} else if (strcasecmp(s, "display") == 0) {
							display(dist_vec_list);
							printf("\nDISPLAY SUCCESS\n");
						} else if (strncasecmp(stdinbuf, "disable", strlen("disable") - 1) == 0) {
							char* pch = strtok(stdinbuf, " ");
							char** toks = NULL;
							int tokitr, serverid;
							toks = malloc(sizeof(char*) * 5);
							for (tokitr = 0; pch != NULL; tokitr++) {
								toks[tokitr] = pch;
								pch = strtok(NULL, " ");
							}

							if (tokitr != 2) {
								fprintf(stderr, "\n%s\n", HELPDISABLE);
							} else {
								serverid = strtol(toks[1], NULL, 10);
								int rv = disable(serverid);
								if (rv == 0)
									printf("\nDISABLE: Invalid neighbor id\n");
								else if (rv == 1)
									printf("\nDISABLE SUCCESS\n");
								else
									printf("\nDISABLE: Neighbor already disabled!\n");
							}
							free(toks);
						} else if (strcasecmp(s, "crash") == 0) {
							breakOuterLoop = 1;
							close(listener);
							struct Dist_Vec *curDV = serverTop->dist_vec_list;
							while (curDV != NULL) {
								close(curDV->sockfd);
								curDV = curDV->next;
							}
							FD_ZERO(&master);
							FD_ZERO(&read_fds);
							freeTopList(serverTop);
							freeNeighborDVList(neighborDVList);
							freeDVList(dist_vec_list);
							printf("\nCRASH SUCCESS\n");
							break;
						} else
							fprintf(stderr, "\nUnknown command!\n");
					}
					// end of received update on STDIN

					// received update on listener socket
					else {
						int data_bytes = 0;
						int rejected = 0;
						uint16_t num_updates, num_updates_short;
						struct sockaddr_storage their_addr;
						socklen_t addr_len = sizeof their_addr;
						void *bin_buf = calloc(MAXDATASIZE, sizeof(char));
						struct sockaddr_in sa;
						if ((nbytes = recvfrom(i, bin_buf, MAXDATASIZE - 1, 0, (struct sockaddr *) &their_addr, &addr_len)) <= 0) {
							perror("RECVFROM");
							break;
						}
						memcpy(&(num_updates_short), bin_buf, 2);
						num_updates = ntohs(num_updates_short);
						data_bytes += 2;
						memcpy(&(sa.sin_port), bin_buf + data_bytes, 2);
						data_bytes += 2;
						memcpy(&(sa.sin_addr), bin_buf + data_bytes, 4);
						data_bytes += 4;

						/**********************************************************
						***********************************************************
						**********DISTANCE VECTOR ALGORITHM IMPLEMENTATION*********
						***********************************************************
						**********************************************************/

						int k, n_id, n_cost = INFINITY;
						struct Dist_Vec *newDistVec, *newNeighborDVList;
						struct Neigh_Dist_Vec *curNeighborDV = neighborDVList;
						for (k = 1; k <= num_updates; k++) {
							struct Dist_Vec *newNeighDistVec = (struct Dist_Vec *) malloc(sizeof(*newNeighDistVec));
							char n_n_ip[128];
							int n_n_cost_old = INFINITY;
							int16_t n_n_port, n_n_cost, n_n_id, n_n_cost_short, n_n_id_short;
							memcpy(&(sa.sin_addr), bin_buf + data_bytes, 4);
							data_bytes += 4;
							memcpy(&(sa.sin_port), bin_buf + data_bytes, 2);
							data_bytes += 4;
							memcpy(&(n_n_id_short), bin_buf + data_bytes, 2);
							data_bytes += 2;
							memcpy(&(n_n_cost_short), bin_buf + data_bytes, 2);
							data_bytes += 2;
							inet_ntop(AF_INET, &(sa.sin_addr), n_n_ip, INET_ADDRSTRLEN);
							n_n_port = ntohs(sa.sin_port);
							n_n_cost = ntohs(n_n_cost_short);
							n_n_id = ntohs(n_n_id_short);

							newNeighDistVec->neighbor_id = n_n_id;
							newNeighDistVec->next_hop = 0;
							newNeighDistVec->cost = n_n_cost;
							newNeighDistVec->sockfd = 0;
							newNeighDistVec->backlog = 0;
							newNeighDistVec->disabled = 0;
							newNeighDistVec->unresponsive = 0;
							newNeighDistVec->next = NULL;
							if (k == 1) {
								n_id = n_n_id;
								newNeighDistVec->server_id = n_id;
								struct Dist_Vec *curDV = serverTop->dist_vec_list;
								while (curDV != NULL) {
									if (curDV->neighbor_id == n_id)
										break;
									curDV = curDV->next;
								}
								if (curDV == NULL || curDV->disabled == 1) {
									free(newNeighDistVec);
									rejected = 1;
									dndisp2 = 1;
									break;
								}
								curDV = serverTop->dist_vec_list;
								while (curDV != NULL) {
									if (curDV->neighbor_id == n_id) {
										n_cost = curDV->cost;
										break;
									} else
										curDV = curDV->next;
								}
								while (curNeighborDV->next != NULL) {
									if (curNeighborDV->neighbor_id == n_id)
										break;
									else
										curNeighborDV = curNeighborDV->next;
								}
								if (curNeighborDV->neighbor_id == n_id) {
									freeDVList(curNeighborDV->dist_vec_list);
								}
								newNeighborDVList = newNeighDistVec;
								system("clear");
								printf("\nRECEIVED A MESSAGE FROM SERVER: %d\n", n_id);
							} else {
								newNeighDistVec->server_id = n_id;
								struct Dist_Vec *curNeighDistVec = newNeighborDVList;
								while (curNeighDistVec->next != NULL) {
									curNeighDistVec = curNeighDistVec->next;
								}
								curNeighDistVec->next = newNeighDistVec;
							}

							if (n_n_id != server_id) {
								struct Dist_Vec *curDV = dist_vec_list;
								while (curDV != NULL) {
									if (curDV->neighbor_id == n_n_id) {
										n_n_cost_old = curDV->cost;
										break;
									}
									curDV = curDV->next;
								}
								if (curDV == NULL) {
									// if neighbor not in list, add it
									newDistVec = (struct Dist_Vec *) malloc(sizeof(*newDistVec));
									newDistVec->server_id = server_id;
									newDistVec->neighbor_id = n_n_id;
									newDistVec->next_hop = n_id;
									newDistVec->cost = n_cost + n_n_cost;
									newDistVec->backlog = 0;
									newDistVec->disabled = 0;
									newDistVec->unresponsive = 0;
									newDistVec->next = NULL;
									curDV = dist_vec_list;
									while (curDV->next != NULL) // goto end of list
										curDV = curDV->next;
									curDV->next = newDistVec; //append to list
								} else {
									// find the minimum cost to neighbor's neighbor from other neighbors
									int minCost = INFINITY;
									int next_hop = 0;
									struct Dist_Vec *neighborList = serverTop->dist_vec_list, *curNeighborsDV;
									while (neighborList != NULL) {
										if (neighborList->neighbor_id != n_id) {
											struct Neigh_Dist_Vec *tempNeighborDV = neighborDVList;
											while (tempNeighborDV->next != NULL) {
												if (tempNeighborDV->neighbor_id == neighborList->neighbor_id) {
													break;
												} else
													tempNeighborDV = tempNeighborDV->next;
											}
											if (tempNeighborDV->neighbor_id == neighborList->neighbor_id && tempNeighborDV->dist_vec_list != NULL) {
												curNeighborsDV = tempNeighborDV->dist_vec_list;
												while (curNeighborsDV->next != NULL) {
													if (curNeighborsDV->neighbor_id == n_n_id)
														break;
													curNeighborsDV = curNeighborsDV->next;
												}
												if (curNeighborsDV->neighbor_id == n_n_id) {
													int old_n_n_cost = INFINITY;
													if (curNeighborsDV->neighbor_id == n_n_id)
														old_n_n_cost = curNeighborsDV->cost;
													if ((old_n_n_cost + neighborList->cost) < minCost) {
														minCost = curNeighborsDV->cost + neighborList->cost;
														next_hop = neighborList->neighbor_id;
													}
												}
											}
										}
										neighborList = neighborList->next;
									}
									// if cost to neighbor's neighbor is minimum via this neighbor, replace cost and next hop via this neighbor in distance vector
									if ((n_cost + n_n_cost) < minCost) {
										curDV->cost = n_cost + n_n_cost;
										curDV->next_hop = n_id;
									} else {
									// else replace with minimum obtained above
										curDV->cost = minCost;
										curDV->next_hop = next_hop;
									}
								}
							}
						}
						free(bin_buf);
						if (rejected != 1) {
							curNeighborDV = neighborDVList;
							while (curNeighborDV->next != NULL) {
								if (curNeighborDV->neighbor_id == n_id)
									break;
								else
									curNeighborDV = curNeighborDV->next;
							}
							if (curNeighborDV->neighbor_id == n_id) {
								curNeighborDV->dist_vec_list = newNeighborDVList;
							}
							// increment receive packet count
							serverTop->numPackets = serverTop->numPackets + 1;

							// reset update backlog for this neighbor
							struct Dist_Vec *curDV = serverTop->dist_vec_list;
							while (curDV != NULL) {
								if (curDV->neighbor_id == n_id) {
									curDV->backlog = 0;
									break;
								} else
									curDV = curDV->next;
							} // end of reset update backlog for this neighbor

							//display new distance vector
							display(dist_vec_list);
						}
					}
					// end of received update on listener socket
				}
			}
		}
		if (selectValue == 0) {
			dndisp = 1;
			// create udp packet with routing table and send to other routers
			if (step(dist_vec_list) != 1)
				fprintf(stderr, "\nCould not send routing update!\n");
			// end of create udp packet with routing table and send to other routers

			// update backlog of each neighbor
			struct Dist_Vec *curDV = serverTop->dist_vec_list;
			while (curDV != NULL) {
				int j;
				for (j = 1; j <= serverTop->num_neighbors; j++, curDV = curDV->next) {
					if (curDV->disabled != 1) {
						curDV->backlog = curDV->backlog + 1;
						// check if neighbor has disconnected
						if (curDV->backlog == 3) {
							curDV->cost = INFINITY; // update neighbor link cost to INFINITY
							curDV->unresponsive = 1;
							recomputeDV(dist_vec_list);
						}
						// end of check if neighbor has disconnected
					}
				}
			}
			// end of update backlog of each neighbor

			//reset timeout
			tv.tv_sec = update_interval;
			tv.tv_usec = 0;
		}
	}
	if (breakOuterLoop == 1) {
		while (1)
			;
	}
}

// function to update link costs
int update(int serverId, int neighborId, int cost, struct Dist_Vec *dist_vec_list) {
	struct Dist_Vec *cur = serverTop->dist_vec_list;
	while (cur != NULL) {
		if (cur->server_id == serverId && cur->neighbor_id == neighborId) {
			cur->cost = cost;
			cur->disabled = 0;
			recomputeDV(dist_vec_list);
			return 1;
		} else
			cur = cur->next;
	}
	return 0;
}

// function to send out distance vector updates to neighbors
int step(struct Dist_Vec *dist_vec_list) {
	recomputeDV(dist_vec_list);
	struct Dist_Vec *curDV = dist_vec_list;
	struct addrinfo sa_dest, *ai_dest, *p;
	int i, nbytes, rv, data_bytes = 0;
	short num_updates = listLength(dist_vec_list);
	uint16_t num_updates_short, server_id_short, cost_short = 0;
	/**********************************************
	*	data structure to hold message packet	  *
	**********************************************/
	void *sendData = calloc((12 * num_updates + 8), sizeof(char));
	char numUpdateString[2], serverPortString[2];

	//*********************************************
	//append header
	struct sockaddr_in sa;
	sa.sin_port = htons(serverPort);
	num_updates_short = htons(num_updates);
	inet_pton(AF_INET, serverIp, &(sa.sin_addr));
	memcpy(sendData, &(num_updates_short), sizeof(short));
	data_bytes += 2;
	memcpy(sendData + data_bytes, &(sa.sin_port), 2);
	data_bytes += 2;
	memcpy(sendData + data_bytes, &(sa.sin_addr), 4);
	data_bytes += 4;
	server_id_short = htons(server_id);

	//*********************************************
	//append self information
	memcpy(sendData + data_bytes, &(sa.sin_addr), 4);
	data_bytes += 4;
	memcpy(sendData + data_bytes, &(sa.sin_port), 2);
	data_bytes += 4;
	memcpy(sendData + data_bytes, &server_id_short, 2);
	data_bytes += 2;
	memcpy(sendData + data_bytes, &cost_short, 2);
	data_bytes += 2;

	while (curDV != NULL) {
		// identify client ip and port from topology file
		struct ServerIP *curNeighbor = serverTop->server_list;
		int j;
		for (j = 1; j <= serverTop->num_servers; j++) {
			if (curNeighbor->server_id == curDV->neighbor_id) {
				break;
			} else
				curNeighbor = curNeighbor->next;
		}
		// end of identification
		if (curDV->neighbor_id != server_id) {
			//*********************************************
			//append neighbor information
			struct sockaddr_in sa;
			sa.sin_port = htons(curNeighbor->port);
			inet_pton(AF_INET, curNeighbor->ip, &(sa.sin_addr));
			server_id_short = htons(curNeighbor->server_id);
			cost_short = htons(curDV->cost);
			memcpy(sendData + data_bytes, &(sa.sin_addr), 4);
			data_bytes += 4;
			memcpy(sendData + data_bytes, &(sa.sin_port), 2);
			data_bytes += 4;
			memcpy(sendData + data_bytes, &server_id_short, 2);
			data_bytes += 2;
			memcpy(sendData + data_bytes, &cost_short, 2);
			data_bytes += 2;
		}
		curDV = curDV->next;
	}

	char remotePortString[10];
	curDV = serverTop->dist_vec_list;
	for (i = 1; i <= serverTop->num_neighbors; i++, curDV = curDV->next) {
		if (curDV->disabled != 1) {
			// identify client ip and port from topology file
			struct ServerIP *curNeighbor = serverTop->server_list;
			int j;
			for (j = 1; j <= serverTop->num_servers; j++) {
				if (curNeighbor->server_id == curDV->neighbor_id) {
					break;
				} else
					curNeighbor = curNeighbor->next;
			}
			// end of identification

			memset(&sa_dest, 0, sizeof(struct addrinfo));
			sa_dest.ai_family = AF_INET;
			sa_dest.ai_socktype = SOCK_DGRAM;
			sprintf(remotePortString, "%d", curNeighbor->port);
			if ((rv = getaddrinfo(curNeighbor->ip, remotePortString, &sa_dest, &ai_dest)) != 0) {
				fprintf(stderr, "\nGETADDRINFO: %s\n", gai_strerror(rv));
				freeaddrinfo(ai_dest);
				continue;
			}
			for (p = ai_dest; p != NULL; p = p->ai_next) {
				if (p->ai_family == AF_INET)
					break;
			}

			if (p == NULL) {
				fprintf(stderr, "\nFailed to find IPV4 address for neighbor\n");
				continue;
			}
			if ((nbytes = sendto(curDV->sockfd, sendData, sizeof(char) * (12 * num_updates + 8), 0, p->ai_addr, p->ai_addrlen)) == -1) {
				perror("SEND");
				continue;
			}
			freeaddrinfo(ai_dest);
		}
	}
	free(sendData);
	return 1;
}

// function to copy a given node in a linked list
struct Dist_Vec *copy(struct Dist_Vec *head) {
	if (head == NULL)
		return NULL;
	struct Dist_Vec *new = (struct Dist_Vec *) malloc(sizeof(struct Dist_Vec));
	new->backlog = head->backlog;
	new->cost = head->cost;
	new->neighbor_id = head->neighbor_id;
	new->next = head->next;
	new->server_id = head->server_id;
	new->sockfd = head->sockfd;
	new->next_hop = head->next_hop;
	new->backlog = head->backlog;
	return new;
}

// function to display current distance vector
void display(struct Dist_Vec *dist_vec_list) {
	struct Dist_Vec *nbPtr;
	printf("\nDISTANCE VECTORS\n");
	printf("\nServer Id \t\tNeighbor Id \t\tNext Hop \t\tLink Cost\n");
	printf("-----------------------------------------------------------------------------------\n\n");
	int k;
	for (k = 1; k <= serverTop->num_servers; k++) {
		struct Dist_Vec *curDV = dist_vec_list;
		while (curDV != NULL) {
			int breakLoop = 0;
			if (curDV->neighbor_id == k) {
				nbPtr = serverTop->dist_vec_list;
				while (nbPtr != NULL) {
					if (nbPtr->neighbor_id == k) {
						if (nbPtr->disabled == 1 && curDV->next_hop == curDV->neighbor_id)
							breakLoop = 1;
						break;
					}
					nbPtr = nbPtr->next;
				}
				if (breakLoop == 1)
					break;
				if (curDV->cost < INFINITY)
					printf("%10d \t\t%11d \t\t%8d \t\t%9d\n", curDV->server_id, curDV->neighbor_id, curDV->next_hop, curDV->cost);
				else
					printf("%10d \t\t%11d \t\t%8d \t\t%9s\n", curDV->server_id, curDV->neighbor_id, curDV->next_hop, INFINITYSTRING);
				break;
			} else
				curDV = curDV->next;
		}
	}
}

// function to compute current distance vector from link costs and neighbors' distance vectors
void recomputeDV(struct Dist_Vec *dist_vec_list) {
	struct Dist_Vec *curDV = dist_vec_list;
	while (curDV != NULL) {
		int n_n_id = curDV->neighbor_id;
		if (n_n_id != server_id) {
			int minCost = INFINITY;
			int next_hop = n_n_id;
			struct Dist_Vec *neighborList = serverTop->dist_vec_list, *curNeighborsDV;
			while (neighborList != NULL) {
				struct Neigh_Dist_Vec *curNeighborDV = neighborDVList;
				while (curNeighborDV->next != NULL) {
					if (curNeighborDV->neighbor_id == neighborList->neighbor_id) {
						break;
					} else
						curNeighborDV = curNeighborDV->next;
				}
				if (curNeighborDV->neighbor_id == neighborList->neighbor_id) {
					curNeighborsDV = curNeighborDV->dist_vec_list;
					while (curNeighborsDV->next != NULL) {
						if (curNeighborsDV->neighbor_id == n_n_id)
							break;
						curNeighborsDV = curNeighborsDV->next;
					}
					int old_n_n_cost = INFINITY;
					if (curNeighborsDV->neighbor_id == n_n_id)
						old_n_n_cost = curNeighborsDV->cost;
					if ((old_n_n_cost + neighborList->cost) < minCost) {
						minCost = curNeighborsDV->cost + neighborList->cost;
						next_hop = neighborList->neighbor_id;
					}
				}
				neighborList = neighborList->next;
			}
			curDV->cost = minCost;
			curDV->next_hop = next_hop;
		}
		curDV = curDV->next;
	}
}

// function to disable link between a particular neighbor
int disable(int neighbor_id) {
	struct Dist_Vec *curDV = serverTop->dist_vec_list;
	while (curDV->next != NULL) {
		if (curDV->neighbor_id == neighbor_id)
			break;
		curDV = curDV->next;
	}
	if (curDV->neighbor_id == neighbor_id) {
		if (curDV->disabled == 1)
			return 2;
		curDV->disabled = 1;
		curDV->cost = INFINITY;
		//remove neighbor's distance vector
		struct Neigh_Dist_Vec *curNeighborDV = neighborDVList;
		while (curNeighborDV->next != NULL) {
			if (curNeighborDV->neighbor_id == curDV->neighbor_id) {
				break;
			} else
				curNeighborDV = curNeighborDV->next;
		}
		if (curNeighborDV->neighbor_id == curDV->neighbor_id) {
			freeDVList(curNeighborDV->dist_vec_list->next);
			curNeighborDV->dist_vec_list->next = NULL;
		}
		recomputeDV(dist_vec_list);
		return 1;
	}
	return 0;
}

// function to free distance vector linked lists
void freeDVList(struct Dist_Vec* head) {
	struct Dist_Vec* tmp;
	if (head != NULL) {
		while (head->next != NULL) {
			tmp = head;
			head = head->next;
			free(tmp);
		}
		free(head);
	}
}

// function to free the linked list holding neigbors' distance vectors
void freeNeighborDVList(struct Neigh_Dist_Vec* head) {
	struct Neigh_Dist_Vec* tmp;
	if (head != NULL) {
		while (head->next != NULL) {
			tmp = head;
			head = head->next;
			freeDVList(tmp->dist_vec_list);
			free(tmp);
		}
		freeDVList(head->dist_vec_list);
		free(head);
	}
}

// function to free the initially read topology
void freeTopList(struct Topology* head) {
	if (head != NULL) {
		struct ServerIP* servHead = head->server_list;
		struct ServerIP* tmp;
		if (servHead != NULL) {
			while (servHead->next != NULL) {
				tmp = servHead;
				servHead = servHead->next;
				free(tmp->ip);
				free(tmp);
			}
			free(servHead->ip);
			free(servHead);
		}
		freeDVList(head->dist_vec_list);
		free(head);
	}
}

// function to display initial cost to neighbors
void displayCost() {
	struct Dist_Vec *nbPtr;
	printf("\nCOST VECTORS\n");
	printf("\nServer Id \t\tNeighbor Id \t\tNext Hop \t\tLink Cost\n");
	printf("-----------------------------------------------------------------------------------\n\n");
	int k;
	for (k = 1; k <= serverTop->num_servers; k++) {
		struct Dist_Vec *curDV = serverTop->dist_vec_list;
		while (curDV != NULL) {
			int breakLoop = 0;
			if (curDV->neighbor_id == k) {
				if (curDV->cost < INFINITY)
					printf("%10d \t\t%11d \t\t%8d \t\t%9d\n", curDV->server_id, curDV->neighbor_id, curDV->next_hop, curDV->cost);
				else
					printf("%10d \t\t%11d \t\t%8d \t\t%9s\n", curDV->server_id, curDV->neighbor_id, curDV->next_hop, INFINITYSTRING);
				break;
			} else
				curDV = curDV->next;
		}
	}
}

// function to compute length of linked list holding distance vectors
int listLength(struct Dist_Vec* item) {
	struct Dist_Vec* cur = item;
	int size = 0;

	while (cur != NULL) {
		++size;
		cur = cur->next;
	}

	return size;
}
