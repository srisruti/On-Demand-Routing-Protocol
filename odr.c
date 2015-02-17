//1. Runs on each of the VM machines
//2. Command Line argument: staleness time parameter
//3. get_hw_addrs: to get the index; IP; ethernet addresses: bind to all the HW addresses
//4. create a Unix domain socket with well-known pathname
//5. create a PF_PACKET socket with :choose a unique protocol number   
//6. create a structure for routing table .
//7. put select on the 2 sockets
//8. to flood ; keep the broadcast destination address in the ethernet frame
//9. reverse route in routing table
//10.generate RREP if destination or intermediate node with route
//11.


#include "hw_addrs.h"
#include "unp.h"
#include<stdlib.h>
#include<string.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

extern struct hwa_info *get_hw_addrs();
void insert_wellknown_path(int,char*);
void print_wellknown_path();
char* get_pathname(int);
int insert_port_num(char*);
char* get_hostname(char*);

int cliport=7001;

#define ETH_49215 
#define ODRDG_PATH "/tmp/odrfile9215"
#define SERVDG_PATH  "/tmp/servfile9215"
#define SERVPORT 5001
#define USID_PROTO 9215


struct route_table
{
	char destCanonicalIP[16];
	int out_interface;
	char nextHopMacAddr[18];
	int hop_count;
	time_t timestamp;;
	int broadcastID;
	struct route_table *next;
}*rhead=NULL,*rcurr=NULL;

struct ODR_message
{
	int type; // 0 for RREQ; 1 for RREP; 2 for data payload
	int rrep_sent_flag; // RREQ
	int hopcount; // RREQ / RREP / Data Payload
	int broadcast_ID; // RREQ
	int force_redisc; // RREQ / RREP
	char srcCanonicalIP[16]; // Data Payload
	int src_port; // Data Payload
	char destCanonicalIP[16]; // Data Payload
	int dest_port; // Data Payload
	int appBytes; // Data Payload: number of bytes in app message
	char data[50];
};
struct PF_PACKET_info
{
	int pfsockfd;
	int hw_index;
	char hw_addr[18];
	char ipaddr[16];
}ha_info[10];


struct wellknown_path
{
	int port;
	char sun_path_file[MAXLINE];
	struct wellknown_path *next;
}*head=NULL,*curr=NULL;

struct store_Data_RREP
{
	char srcCanonicalIP[16];
	char destCanonicalIP[16];  	
	struct ODR_message odrm;
	struct store_Data_RREP *next; 	
}*shead=NULL,*scurr=NULL;

int traverse_route_table(char*,int*,char*,int*,int*,int);
void broadcast_to_ODR(int,struct ODR_message odrm,int);
void fill_ODR_message(struct ODR_message *odrm,int,int,int,int,int,char*,int,char*,int,char*);
void send_to_ODR(int,struct ODR_message odrm,int,char*);
void insertinto_route_table(char*,int,char*,int,time_t,int);
void update_route_table(char*,int,char*,int,int,time_t);
void print_routing_table();
void insert_into_store_Data_RREP(char*,char*,struct ODR_message odrm);
void delete_store_Data_RREP(char*);
void print_store_Data_RREP();
void delete_routing_table(char*,int);

int main(int argc,char* argv[])
{
	struct hwa_info *hwa;
	//struct PF_PACKET_info ha_info[10];
	struct ODR_message odr_msg;
	struct sockaddr  *sa;
	char *addr,*ptr;
	int i=0;
	int j=0,m=0;

	int sockfd;
	struct sockaddr_un odraddr,destaddr,srcaddr; // remove the cliaddr,servaddr (redundant)
	char msgrecv[MAXLINE],srcmsg[MAXLINE]; 
	char canonicalIP[16]; // canonical IP address of the ODR
	socklen_t destlen,srclen; // remove clilen and servlen
	char *srcaddrbuf; // to get the address of the source/dest (change it)
	char hwaddress[19]; 
	int n=0,len=0;
	char temp[4]; // to store the MAC address
		
	char *srctok;
	char destCanonicalIP[16],destmsg[MAXLINE]; // 
	int destport,srcflag,srcsockfd,srcport;
	int buflen=0; 
	
	char recvfilename[MAXLINE];
	
	socklen_t hlen;
        struct sockaddr_ll ethaddr,haddr;
	
	int maxfdp1;
	fd_set rset;
	int structlen,l;

	void* odrbuff=(void*)malloc(ETH_FRAME_LEN);
    unsigned char* data;
	unsigned char* etherhead;
	struct ODR_message odrecv_msg;	
	struct sockaddr_ll odrecv;
	socklen_t odrecvlen;
	char odrdestmac[MAXLINE];
	char odrsrcmac[MAXLINE];
	char srcodrmsg[MAXLINE];

	int routeExists,broadcastID=0,odrmsgtyp;
	int hopcount=1;
	int outInterface;
	int rrep_sent=0;
	char nextHopMac[18];
    int routeBID,rhopcnt;    

	time_t currtime;
	int staleness;

	struct store_Data_RREP sdrp;
	int resent=0;
 

	if(argc!=2)
		err_quit("Staleness parameter not entered.\n");
	
	staleness=atoi(argv[1]);

	if((hwa=get_hw_addrs())==NULL)
		err_sys("get_hw_addrs error.");
	
	fprintf(stdout,"============ODR STARTED============\n\n");
	fprintf(stdout,"Interface details of the ODR:\n");
	
	for(;hwa!=NULL;hwa=hwa->hwa_next)
	{
		
		if((strcmp(hwa->if_name,"eth0")==0))
		{
			//fprintf(stdout,"eth0 are not considered.\n");
			sa=hwa->ip_addr;
			memset(&canonicalIP[0], 0, 16);
			//strcpy(canonicalIP,"");
			strcpy(canonicalIP,sock_ntop_host(sa,sizeof(sa)));
			fprintf(stdout,"The canonical IP address of ODR:%s\n\n",canonicalIP);
			continue;
		}
		if((strcmp(hwa->if_name,"lo")==0))
		{
			//fprintf(stdout,"lo interfaces are not considered.\n");
			continue;
		}
		
			
		printf("Interface name:%s\n",hwa->if_name);
		sa=hwa->ip_addr;
		memset(&ha_info[i].ipaddr[0], 0, sizeof(ha_info[i].ipaddr));
		//strcpy(ha_info[i].ipaddr,"");
		strcpy(ha_info[i].ipaddr,sock_ntop_host(sa,sizeof(sa)));
		fprintf(stdout,"The IP address is:%s\n",ha_info[i].ipaddr);
		
		ha_info[i].hw_index=hwa->if_index;
		fprintf(stdout,"The interface index is:%d\n",ha_info[i].hw_index);
		
		memset(&hwaddress[0], 0, sizeof(hwaddress));
		ptr = hwa->if_haddr;
      	int k = 6;
      	do {
      		sprintf(temp,"%.2x%s", *ptr++ & 0xff, (k == 1) ? " " : ":");	
			strncat(hwaddress,temp,3);
       	} while (--k > 0);
		hwaddress[18]='\0';

		memset(&ha_info[i].hw_addr[0], 0, sizeof(ha_info[i].hw_addr));
		strncpy(ha_info[i].hw_addr,hwaddress,strlen(hwaddress)-1);
		fprintf(stdout,"The MAC address is:%s\n",ha_info[i].hw_addr);
		printf("\n"); 
                
		//fprintf(stdout,"------------Before PF_PACKET creation----------------\n");
		if((ha_info[i].pfsockfd=socket(PF_PACKET,SOCK_RAW,htons(USID_PROTO)))<0)
			err_sys("PF_PACKET socket creation error.");
		
		//fprintf(stdout,"------------After PF_PACKET creation.................\n");
                ethaddr.sll_family=PF_PACKET;
		ethaddr.sll_protocol=htons(USID_PROTO);
		ethaddr.sll_ifindex=hwa->if_index;
		ethaddr.sll_halen=ETH_ALEN;
		//fprintf(stdout,"------------Before PF_PACKET Bind---------------------\n");
		if((bind(ha_info[i].pfsockfd,(struct sockaddr_ll *)&ethaddr,sizeof(ethaddr)))<0)
			err_sys("PF_PACKET socket bind error.");
		//fprintf(stdout,"------------After PF_PACKET Bind---------------------\n");
		len=sizeof(haddr);
		//fprintf(stdout,"------------Before PF_PACKET GetSockname---------------------\n");
		if((getsockname(ha_info[i].pfsockfd,(SA *)&haddr,&len))<0)
			err_sys("PF_PACKET getsockname error.");
		//fprintf(stdout,"------------After PF_PACKET Getsockname---------------------\n");
		//fprintf(stdout,"Binded the PF_SOCKET %d to index %d\n",ha_info[i].pfsockfd,haddr.sll_ifindex);
	
		i++;
		fprintf(stdout,"\n\n");

	}
	
	structlen=i;

	insert_wellknown_path(SERVPORT,SERVDG_PATH);
	//print_wellknown_path();

	if((sockfd=socket(AF_LOCAL,SOCK_DGRAM,0))<0)
		err_sys("unix domain ODR socket creation error.");
	odraddr.sun_family=AF_LOCAL;
	memset(&odraddr.sun_path[0], 0, sizeof(odraddr.sun_path));
	//strcpy(odraddr.sun_path,"");
	strcpy(odraddr.sun_path,ODRDG_PATH);
	//fprintf(stdout,"The ODR pathname is:%s\n",odraddr.sun_path);
	unlink(ODRDG_PATH);
	
	if((bind(sockfd,(SA*) &odraddr,sizeof(odraddr)))<0)
		err_sys("bind ODR unix domain socket error.");
	
	memset(&nextHopMac[0], 0, sizeof(nextHopMac));
	//strcpy(nextHopMac,"");
	FD_ZERO(&rset);
	for(;;)
	{
		//fprintf(stdout,"Waiting for sources...\n");
		FD_SET(sockfd,&rset);
		for(l=0;l<structlen;l++)
		{
			FD_SET(ha_info[l].pfsockfd,&rset);
			maxfdp1=max(maxfdp1,ha_info[l].pfsockfd);
		}

		maxfdp1=max(maxfdp1,sockfd)+1;

		selectagain:
		if((select(maxfdp1,&rset,NULL,NULL,NULL))<0)
		{
			if(errno==EINTR)
				goto selectagain;
		}
		//fprintf(stdout,"After select ..\n\n");
		if(FD_ISSET(sockfd,&rset))
		{
            srclen=sizeof(srcaddr);
		
            if((n=recvfrom(sockfd,msgrecv,MAXLINE,0,(SA *) &srcaddr, &srclen))<0)
                    err_sys(" odr recvfrom error.");

           	//fprintf(stdout,"Message from the source:%s\n",msgrecv);
			srcaddrbuf=(char *)malloc(srclen);
			strncpy(srcaddrbuf,(sock_ntop_host((SA *) &srcaddr,srclen)),srclen);
			//fprintf(stdout,"The source address:%s\n",srcaddrbuf);
			srcport=insert_port_num(srcaddrbuf);
			//print_wellknown_path();
			//fprintf(stdout,"SRCPORT:%d\n",srcport);
          
			srctok=strtok(msgrecv,";");
			srcsockfd=atoi(srctok);
			//fprintf(stdout,"Source socket file descriptor:%s\n",srctok);
			srctok=strtok(NULL,";");
			strncpy(destCanonicalIP,srctok,strlen(srctok));
			//fprintf(stdout,"Destination canonical IP address is:%s\n",destCanonicalIP);
			destport=atoi(strtok(NULL,";"));
			//fprintf(stdout,"Destination port number:%d\n",destport);
			srctok=strtok(NULL,";");
			//printf("Length of data message is: [%d] in token\n", strlen(srctok));
			//destmsg=(char *)malloc(strlen(srctok));
			strncpy(destmsg,srctok,strlen(srctok));
			//fprintf(stdout,"Data to be sent to the destination[%s]\n",destmsg);
			srcflag=atoi(strtok(NULL,";"));
			//fprintf(stdout,"The client set flag is:%d\n",srcflag);
		
			if(strcmp(canonicalIP,destCanonicalIP)==0) // to check if the destination is on the same node as the source
			{
				memset(&recvfilename[0], 0, MAXLINE);
				//strcpy(recvfilename,"");
				strcpy(recvfilename,get_pathname(destport));
				//fprintf(stdout,"The destination pathname:%s\n\n",recvfilename);
			
				buflen=0;
				//fprintf(stdout,"\nThe server to which the request is to be sent is on my node.\n");
				memset(&srcmsg[0], 0, sizeof(srcmsg));
				//strcpy(srcmsg,"");
				sprintf(srcmsg+buflen,"%d",srcport);
				buflen=strlen(srcmsg);
				sprintf(srcmsg+buflen,";%s",destmsg);
				buflen=strlen(srcmsg);
				sprintf(srcmsg+buflen,";%s",canonicalIP);
			
				bzero(&destaddr,sizeof(destaddr));
				destaddr.sun_family=AF_LOCAL;
				memset(&destaddr.sun_path[0], 0, sizeof(destaddr.sun_path));
				//strcpy(destaddr.sun_path,"");
				strcpy(destaddr.sun_path,recvfilename);
				destlen=sizeof(struct sockaddr_un);
				Sendto(sockfd,srcmsg,MAXLINE,0,(SA *)&destaddr,destlen);
				fprintf(stdout,"\nSent request to the destination.\n");
			}
			else // source and destination are on different nodes
			{
				// check if there is a route in the routing table
				routeExists=traverse_route_table(destCanonicalIP,&outInterface,nextHopMac,&routeBID,&rhopcnt,staleness);
				if(routeExists==1) // then send the message along the route in the routing table.
				{
					odrmsgtyp=2;
					//fprintf(stdout,"There is a route to the destination of DATA in the routing table at the source ODR.\n");
					fill_ODR_message(&odr_msg,odrmsgtyp,0,hopcount,0,0,canonicalIP,srcport,destCanonicalIP,destport,destmsg);
					
					send_to_ODR(structlen,odr_msg,outInterface,nextHopMac);
				
					fprintf(stdout,"\nODR at node %s : Sending frame header, Source %s and Destination %s\n",get_hostname(canonicalIP),get_hostname(canonicalIP),nextHopMac);
                    printf("\nODR message: Type %d, Source: %s ,",odrmsgtyp,get_hostname(canonicalIP));
                    printf("Destination:%s\n",get_hostname(destCanonicalIP));
				}
				else // there is no route in the routing table
				{
				 	//Broadcast the RREQ 
					// park the data
					//fprintf(stdout,"\nThere is no route to the destination of DATA in the routing table at the source node.\n");
					//fprintf(stdout,"\nParking the data\n");
					odrmsgtyp=2;
                    fill_ODR_message(&odr_msg,odrmsgtyp,0,hopcount,0,0,canonicalIP,srcport,destCanonicalIP,destport,destmsg);
					insert_into_store_Data_RREP(canonicalIP,destCanonicalIP,odr_msg);
					//print_store_Data_RREP();
					odrmsgtyp=0;
					//fprintf(stdout,"Broadcasting the RREQ.....\n");
					fill_ODR_message(&odr_msg,odrmsgtyp,rrep_sent,hopcount,++broadcastID,srcflag,canonicalIP,0,destCanonicalIP,0,"Nothing");
					
					fprintf(stdout,"\nODR at node %s : Sending frame header, Source %s and Destination %s\n",\
									get_hostname(canonicalIP),get_hostname(canonicalIP),"ff:ff:ff:ff:ff:ff");
					printf("\nODR message: Type %d, Source: %s ,",odrmsgtyp,get_hostname(canonicalIP));
					printf("Destination:%s\n",get_hostname(destCanonicalIP)); 

					broadcast_to_ODR(structlen,odr_msg,0);
				}
			}
		}
		for(j=0;j<structlen;j++)
		{	
			//fprintf(stdout,"Inside loop .....\n");
			if(FD_ISSET(ha_info[j].pfsockfd,&rset)) // To receive from other ODRs
			{
				odrecvlen=sizeof(odrecv);
			
				if((recvfrom(ha_info[j].pfsockfd,odrbuff,ETH_FRAME_LEN,0,(SA *)&odrecv,&odrecvlen))<0)
					err_sys("ODR to ODR recvfrom error."); 
					
				//fprintf(stdout,"\n\nThe index of the ODR recv  %d\n",ha_info[j].hw_index);
				
				data=odrbuff+14;
				memcpy((void*) &odrecv_msg, (void*) data,sizeof(struct ODR_message));
			
				/*etherhead=odrbuff;
				memcpy((void*) odrdestmac,(void*)odrbuff,ETH_ALEN);
				memcpy((void*) odrsrcmac,(void*)(odrbuff+ETH_ALEN),ETH_ALEN);*/
				//fprintf(stdout,"MAC address of the source ODR:%s\n",odrsrcmac);
				
				sprintf(odrsrcmac,"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",odrecv.sll_addr[0],odrecv.sll_addr[1],\
							odrecv.sll_addr[2],odrecv.sll_addr[3],odrecv.sll_addr[4],odrecv.sll_addr[5]);
			
				//fprintf(stdout,"\nMAC address of the source ODR:%s and index : %d\n",odrsrcmac,odrecv.sll_ifindex);

		 	
			        	
				if(odrecv_msg.type==0) // If it is RREQ
				{
					// Checking if there is an entry to the source in the routing table
					//fprintf(stdout,"\nRECEIVED RREQ\n");
					
					if(strcmp(odrecv_msg.srcCanonicalIP,canonicalIP)==0)
					{	
						//fprintf(stdout,"\nReceived the RREQ which I broadcasted.\n");
						break;
					}
					routeExists=traverse_route_table(odrecv_msg.srcCanonicalIP,&outInterface,nextHopMac,&routeBID,&rhopcnt,staleness);
					if(routeExists==1)
					{
							if(odrecv_msg.broadcast_ID<routeBID) // ignore the BID do not flood it
							{
							 	break;
								// exit(0);  
							}
							else if(odrecv_msg.broadcast_ID>routeBID) // a new RREQ from the source
							{
								insertinto_route_table(odrecv_msg.srcCanonicalIP,odrecv.sll_ifindex,odrsrcmac,\
															odrecv_msg.hopcount,time(NULL),odrecv_msg.broadcast_ID);
                        		print_routing_table();	
							}
							else if(odrecv_msg.broadcast_ID==routeBID) // same RREQ
							{
								if(odrecv_msg.hopcount<rhopcnt)  // a better route to the source
								{
			
									fprintf(stdout,"\nUpdating the hop count for route of:%s\n",odrecv_msg.srcCanonicalIP); 
									update_route_table(odrecv_msg.srcCanonicalIP,odrecv.sll_ifindex,odrsrcmac,\
															odrecv_msg.hopcount,odrecv_msg.broadcast_ID,time(NULL));
									print_routing_table();
								}
								else if(odrecv_msg.hopcount>rhopcnt)
								{
									break;
									// exit(0);
								}
								else if(odrecv_msg.hopcount==rhopcnt)
								{	
										update_route_table(odrecv_msg.srcCanonicalIP,odrecv.sll_ifindex,odrsrcmac,odrecv_msg.hopcount,\
															odrecv_msg.broadcast_ID,time(NULL));
										break;
										
								}
								
							}
								
					}
					else // new route for the source
					{
						insertinto_route_table(odrecv_msg.srcCanonicalIP,odrecv.sll_ifindex,odrsrcmac,\
													odrecv_msg.hopcount,time(NULL),odrecv_msg.broadcast_ID);
                        print_routing_table();

					}
					if(strcmp(canonicalIP,odrecv_msg.destCanonicalIP)==0) // Request for destination is on my node
                    {	
						//fprintf(stdout,"\nReceived RREQ, Destination is on my node.\n");
						if(odrecv_msg.rrep_sent_flag==0) // RREP is not sent by any intermediate node
						{	
							//fprintf(stdout,"\nThe RREP already sent flag is 0.\n");
							//fprintf(stdout,"\nGenerating RREP.\n");
						//generate RREP
						odrmsgtyp=1;
						fill_ODR_message(&odr_msg,odrmsgtyp,0,hopcount,0,srcflag,odrecv_msg.destCanonicalIP,\
											srcport,odrecv_msg.srcCanonicalIP,destport,"Nothing");
					// Sending the RREP from the same index and to the same source from which the RREQ was received

						fprintf(stdout,"\nODR at node %s : Sending frame header, Source %s and Destination %s\n",\
														get_hostname(canonicalIP),get_hostname(canonicalIP),odrsrcmac);
                    printf("\nODR message: Type %d, Source: %s ,",odrmsgtyp,get_hostname(odrecv_msg.destCanonicalIP));
                    printf("Destination:%s\n",get_hostname(odrecv_msg.srcCanonicalIP));		

						send_to_ODR(structlen,odr_msg,odrecv.sll_ifindex,odrsrcmac);

						//fprintf(stdout,"\nSent the RREP.\n"); 
						}
						else // RREP is already sent by an intermediate node
						{
							//fprintf(stdout,"\nThe RREP is already sent by an intermediate node.\n");
							break;
						}
						// add to routing table
						
					}
					else  // Intermediate node receiving the RREQ
					{ 
						//fprintf(stdout,"\nReceived RREQ, Destination is not on my node.\n");
						// Check if there is an entry for the destination in the routing table.
						routeExists=traverse_route_table(odrecv_msg.destCanonicalIP,&outInterface,nextHopMac,&routeBID,&rhopcnt,staleness);
						if(routeExists==1) // There is a route entry to the destination
						{	
							 //fprintf(stdout,"\nI am the intermediate node and I have a route to the destination in my routing table.\n");
							// generate RREP and send it to the source
							if(odrecv_msg.rrep_sent_flag==0) // RREP is not already sent
							{
						
                            	//fprintf(stdout,"\nRREP already sent flag is 0. So sending RREP to the source.\n");	
	  							odrmsgtyp=1;
                        		fill_ODR_message(&odr_msg,odrmsgtyp,0,hopcount,0,srcflag,odrecv_msg.destCanonicalIP,0,odrecv_msg.srcCanonicalIP,0,"Nothing");
                    // Sending the RREP from the same index and to the same source from which the RREQ was received
								fprintf(stdout,"\nODR at node %s : Sending frame header, Source %s and Destination %s\n",\
																get_hostname(canonicalIP),get_hostname(canonicalIP),odrsrcmac);
                    			printf("\nODR message: Type %d, Source: %s ,",odrmsgtyp,get_hostname(odrecv_msg.destCanonicalIP));
                    			printf("Destination:%s\n",get_hostname(odrecv_msg.srcCanonicalIP));
                        		
								send_to_ODR(structlen,odr_msg,odrecv.sll_ifindex,odrsrcmac);
                        		
							//fprintf(stdout,"\nSent the RREP.\n");

							// Broadcast the RREQ but set the RREP already sent flag to 1
								rrep_sent=1;
								//fprintf(stdout,"\nBroadcasting the RREQ with already sent flag set to 1.\n");
								
								fill_ODR_message(&odr_msg,odrecv_msg.type,rrep_sent,++odrecv_msg.hopcount,\
										odrecv_msg.broadcast_ID,odrecv_msg.force_redisc,odrecv_msg.srcCanonicalIP,\
											odrecv_msg.src_port,odrecv_msg.destCanonicalIP,odrecv_msg.dest_port,"Nothing");
							
								
								fprintf(stdout,"\nODR at node %s : Sending frame header, Source %s and Destination %s\n",get_hostname(canonicalIP),get_hostname(canonicalIP),"ff:ff:ff:ff:ff:ff");
                                printf("\nODR message: Type %d, Source: %s ,",odrecv_msg.type,get_hostname(odrecv_msg.srcCanonicalIP));
                                printf("Destination:%s\n",get_hostname(odrecv_msg.destCanonicalIP));

								broadcast_to_ODR(structlen,odr_msg,odrecv.sll_ifindex);
							}
							else // RREP is already sent, so just broadcast the RREQ
							{ 
								//fprintf(stdout,"\nRREP already sent flag is 1. So broadcasting the RREQ.\n");
								
								fill_ODR_message(&odr_msg,odrecv_msg.type,odrecv_msg.rrep_sent_flag,\
										++odrecv_msg.hopcount,odrecv_msg.broadcast_ID,odrecv_msg.force_redisc,\
											odrecv_msg.srcCanonicalIP,odrecv_msg.src_port,odrecv_msg.destCanonicalIP,\
													odrecv_msg.dest_port,"Nothing");

								fprintf(stdout,"\nODR at node %s : Sending frame header, Source %s and Destination %s\n",get_hostname(canonicalIP),get_hostname(canonicalIP),"ff:ff:ff:ff:ff:ff");
                                printf("\nODR message: Type %d, Source: %s ,",odrecv_msg.type,get_hostname(odrecv_msg.srcCanonicalIP));
                                printf("Destination:%s\n",get_hostname(odrecv_msg.destCanonicalIP));

                                broadcast_to_ODR(structlen,odr_msg,odrecv.sll_ifindex);
							}
							
						}
						else // there is no route in the routing table 
						{	
							// fprintf(stdout,"\nThere is no route to the destination in my routing table.\n");
							// no route in the routing table
							// broadcast the RREQ again; but not from the same interface

							fill_ODR_message(&odr_msg,odrecv_msg.type,odrecv_msg.rrep_sent_flag,\
								++odrecv_msg.hopcount,odrecv_msg.broadcast_ID,odrecv_msg.force_redisc,\
									odrecv_msg.srcCanonicalIP,odrecv_msg.src_port,odrecv_msg.destCanonicalIP,\
											odrecv_msg.dest_port,"Nothing");

							fprintf(stdout,"\nODR at node %s : Sending frame header, Source %s and Destination %s\n",get_hostname(canonicalIP),get_hostname(canonicalIP),"ff:ff:ff:ff:ff:ff");
							printf("\nODR message: Type %d, Source: %s ,",odrecv_msg.type,get_hostname(odrecv_msg.srcCanonicalIP));
							printf("Destination:%s\n",get_hostname(odrecv_msg.destCanonicalIP));
							
							broadcast_to_ODR(structlen,odr_msg,odrecv.sll_ifindex);

						}
					}

				}
				else if(odrecv_msg.type==1) // RREP
				{


					//fprintf(stdout,"\nReceived RREP.\n");
					//fprintf(stdout,"\nThe Destination(source) Canonical IP:%s\n",odrecv_msg.destCanonicalIP);   
					//Check if there are any pending data in the linked list to be sent
					
					routeExists=traverse_route_table(odrecv_msg.srcCanonicalIP,&outInterface,nextHopMac,&routeBID,&rhopcnt,staleness);
					if(routeExists==1) 
					{
						if(odrecv_msg.hopcount<rhopcnt)
						{
							if(strcmp(canonicalIP,odrecv_msg.destCanonicalIP)==0)// Destination for RREP(source)
							{
								update_route_table(odrecv_msg.srcCanonicalIP,odrecv.sll_ifindex,odrsrcmac,odrecv_msg.hopcount,odrecv_msg.broadcast_ID,time(NULL));
								break;
							}
							else
							{
								fprintf(stdout,"\nUpdating the hop count for route of:%s\n",odrecv_msg.srcCanonicalIP);
								update_route_table(odrecv_msg.srcCanonicalIP,odrecv.sll_ifindex,odrsrcmac,odrecv_msg.hopcount,odrecv_msg.broadcast_ID,time(NULL));	
								print_routing_table();
							}
						}
						else if(odrecv_msg.hopcount>rhopcnt)
						{
							break;
						}
						else if(odrecv_msg.hopcount==rhopcnt)
						{
							update_route_table(odrecv_msg.srcCanonicalIP,odrecv.sll_ifindex,odrsrcmac,odrecv_msg.hopcount,odrecv_msg.broadcast_ID,time(NULL));
							break;
						}
					}
					else // there is no route to the source(destination) in the routing table
					{
						insertinto_route_table(odrecv_msg.srcCanonicalIP,odrecv.sll_ifindex,odrsrcmac,odrecv_msg.hopcount,time(NULL),0);
						print_routing_table();
					}

					struct store_Data_RREP *ptr=shead;
					while(ptr)
					{
						//fprintf(stdout,"\n While loop....\n");	
						if(strcmp(ptr->destCanonicalIP,odrecv_msg.srcCanonicalIP)==0)
						{
							//fprintf(stdout,"\n There are parked messages for the destination node.\n");
							
							fprintf(stdout,"\nODR at node %s : Sending frame header, Source %s and Destination %s\n",get_hostname(canonicalIP),get_hostname(canonicalIP),odrsrcmac);
                                printf("\nODR message: Type %d, Source: %s ,",ptr->odrm.type,get_hostname(ptr->srcCanonicalIP));
                                printf("Destination:%s\n",get_hostname(ptr->destCanonicalIP));

							send_to_ODR(structlen,ptr->odrm,odrecv.sll_ifindex,odrsrcmac);
							//fprintf(stdout,"\n The data parked is sent to the destination node.\n");
							delete_store_Data_RREP(odrecv_msg.srcCanonicalIP);
							resent=1;
						}
						ptr=ptr->next;	
					}
					// Checking if there is route to the source(destination/ intermediate node) in the routing table

					if(resent==1) // Sent the data/RREP from the list of parked data
					{
						//fprintf(stdout,"\nAlready RREP/Data is sent from the parked data.\n");
						resent=0;
						break;
					}
					if(strcmp(canonicalIP,odrecv_msg.destCanonicalIP)==0) // RREP destination is on my node
					{
						//fprintf(stdout,"\nReceived RREP, Destination for the RREP is on my node.\n");
						
						//fprintf(stdout,"\nSending Data.\n");
                       	odrmsgtyp=2;
                
                     	fill_ODR_message(&odr_msg,odrmsgtyp,0,hopcount,0,0,canonicalIP,srcport,odrecv_msg.srcCanonicalIP,destport,destmsg);

                        // Sending the data from the same index and to the same source from which the RREQ was received
                        
						fprintf(stdout,"\nODR at node %s : Sending frame header, Source %s and Destination %s\n",get_hostname(canonicalIP),get_hostname(canonicalIP),odrsrcmac);
                            printf("\nODR message: Type %d, Source: %s ,",odrmsgtyp,get_hostname(canonicalIP));
                            printf("Destination:%s\n",get_hostname(odrecv_msg.srcCanonicalIP));
						send_to_ODR(structlen,odr_msg,odrecv.sll_ifindex,odrsrcmac);
                        
						//fprintf(stdout,"\nSent the Data.\n");
					
					} 
					else //RREP destination is not on my node
					{
						//fprintf(stdout,"\nThe destination(source) is not my node.\n");
						
						// check if there is a route to the destination(source) in routing table
						routeExists=traverse_route_table(odrecv_msg.destCanonicalIP,&outInterface,nextHopMac,&routeBID,&rhopcnt,staleness);

						//fprintf(stdout,"\nNextHopMac:%s\n",nextHopMac);
					 	if(routeExists==1) 
						{
							// Unicast the RREP
							//fprintf(stdout,"\nThe route to the RREP destination is in the routing table\n");
							//fprintf(stdout,"\n Unicasting the RREP\n");
							
							
							fill_ODR_message(&odr_msg,odrecv_msg.type,odrecv_msg.rrep_sent_flag,++odrecv_msg.hopcount,\
									odrecv_msg.broadcast_ID,odrecv_msg.force_redisc,odrecv_msg.srcCanonicalIP,odrecv_msg.src_port,\
											odrecv_msg.destCanonicalIP,odrecv_msg.dest_port,odrecv_msg.data);	
						
 //	fprintf(stdout,"MSG TYPE:%d, SRC IP: %s, DEST IP:%s, ACTL DEST IP:%s\n",odrecv_msg.type,odrecv_msg.srcCanonicalIP,odrecv_msg.destCanonicalIP,odrecv_msg.actlDestCanIP);
					
							//fprintf(stdout,"OUT INT: %d NEXT HOP:%s\n",outInterface,nextHopMac);

							fprintf(stdout,"\nODR at node %s : Sending frame header, Source %s and Destination %s\n",get_hostname(canonicalIP),get_hostname(canonicalIP),nextHopMac);
                            printf("\nODR message: Type %d, Source: %s ,",odrecv_msg.type,get_hostname(odrecv_msg.srcCanonicalIP));
                            printf("Destination:%s\n",get_hostname(odrecv_msg.destCanonicalIP));
							send_to_ODR(structlen,odr_msg,outInterface,nextHopMac);
							
							//fprintf(stdout,"\nSent the RREP .\n");
						}	
						else  // There is no route to the destination(source) in the routing table
						{
							//fprintf(stdout,"There is no route to the destination of RREP in the routing table at the ODR.\n");
							//fprintf(stdout,"\nParking the RREP.\n");
							insert_into_store_Data_RREP(odrecv_msg.srcCanonicalIP,odrecv_msg.destCanonicalIP,odr_msg);
							//print_store_Data_RREP();	

							odrmsgtyp=0;
							broadcastID=1;

							//fprintf(stdout,"Sending the RREQ again.\n");

                    		fill_ODR_message(&odr_msg,odrmsgtyp,0,hopcount,++broadcastID,srcflag,canonicalIP,0,odrecv_msg.destCanonicalIP,0,"Nothing");
							fprintf(stdout,"\nODR at node %s : Sending frame header, Source %s and Destination %s\n",get_hostname(canonicalIP),get_hostname(canonicalIP),"ff:ff:ff:ff:ff:ff");
                            printf("\nODR message: Type %d, Source: %s ,",odrmsgtyp,get_hostname(canonicalIP));
                            printf("Destination:%s\n",get_hostname(odrecv_msg.destCanonicalIP));
                    		broadcast_to_ODR(structlen,odr_msg,0);
		
						}
					}
					
				} 
				
				else if(odrecv_msg.type==2) // Data Payload
				{
					//printf("Message ready to be sent to Client: {%drecv_msg.data);
					//fprintf(stdout,"\nReceived DATA \n");

					routeExists=traverse_route_table(odrecv_msg.srcCanonicalIP,&outInterface,nextHopMac,&routeBID,&rhopcnt,staleness);
					if(routeExists==1) // There is a route to the source of the data
					{
						//fprintf(stdout,"\nThere is a route to the source of the data.\n");
						//fprintf(stdout,"\nUpdating the timestamp and new hop count.\n");
						// jus update the timestamp
						update_route_table(odrecv_msg.srcCanonicalIP,odrecv.sll_ifindex,odrsrcmac,odrecv_msg.hopcount,odrecv_msg.broadcast_ID,time(NULL));
						//print_routing_table();
						
					}	
                    if(routeExists==0)
                    {
							//fprintf(stdout,"\nThere is no route to the Source of the data\n.");
                            insertinto_route_table(odrecv_msg.srcCanonicalIP,odrecv.sll_ifindex,odrsrcmac,odrecv_msg.hopcount,time(NULL),0);
                            print_routing_table();
                    }

					// Checking if its the ODR of the destination for the data
					if(strcmp(canonicalIP,odrecv_msg.destCanonicalIP)==0)
                   	{
						//fprintf(stdout,"\n Received DATA, Destination for DATA is on my node.\n");
                        //strcpy(recvfilename,"");
						memset(&recvfilename[0], 0, sizeof(recvfilename));
						//fprintf(stdout,"Destination Port for DATA:%d\n",odrecv_msg.dest_port);
						strcpy(recvfilename,get_pathname(odrecv_msg.dest_port));
                       	//fprintf(stdout,"The destination pathname:%s\n",recvfilename);

						buflen=0;
						memset(&srcodrmsg[0], 0, sizeof(srcodrmsg));
                        //strcpy(srcodrmsg,"");
                        sprintf(srcodrmsg+buflen,"%d",odrecv_msg.src_port);
                        buflen=strlen(srcodrmsg);
                        sprintf(srcodrmsg+buflen,";%s",odrecv_msg.data);
                        buflen=strlen(srcodrmsg);
                        sprintf(srcodrmsg+buflen,";%s",odrecv_msg.srcCanonicalIP);
                        
                        bzero(&destaddr,sizeof(destaddr));
                        destaddr.sun_family=AF_LOCAL;
						memset(&destaddr.sun_path[0], 0, sizeof(destaddr.sun_path));
                        //strcpy(destaddr.sun_path,"");
                        strcpy(destaddr.sun_path,recvfilename);
                        destlen=sizeof(struct sockaddr_un);
                        sendto(sockfd,srcodrmsg,MAXLINE,0,(SA *)&destaddr,destlen);
                        
						//fprintf(stdout,"\nSent DATA to the destination:%s\n",srcodrmsg);
					}
					else
					{
						//fprintf(stdout,"\nDestination for DATA is not on my node.\n");
			
						// Check if the route exists to the destination
						
						routeExists=traverse_route_table(odrecv_msg.destCanonicalIP,&outInterface,nextHopMac,&routeBID,&rhopcnt,staleness);
						//fprintf(stdout,"ROUTE EXISTS: %d, OUTINT: %d, NEXTHOPMAC:%s\n",routeExists,outInterface,nextHopMac);
						if(routeExists==1)
						{
							//fprintf(stdout,"\nThere is a route to the destination of the data.\n");
							
							fill_ODR_message(&odr_msg,odrecv_msg.type,odrecv_msg.rrep_sent_flag,++odrecv_msg.hopcount,\
								odrecv_msg.broadcast_ID,odrecv_msg.force_redisc,odrecv_msg.srcCanonicalIP,odrecv_msg.src_port,\
									odrecv_msg.destCanonicalIP,odrecv_msg.dest_port,odrecv_msg.data);
						

							fprintf(stdout,"\nODR at node %s : Sending frame header, Source %s and Destination %s\n",get_hostname(canonicalIP),get_hostname(canonicalIP),nextHopMac);
                            printf("\nODR message: Type %d, Source: %s ,",odrecv_msg.type,get_hostname(odrecv_msg.srcCanonicalIP));
                            printf("Destination:%s\n",get_hostname(odrecv_msg.destCanonicalIP));

							send_to_ODR(structlen,odrecv_msg,outInterface,nextHopMac);	
							//fprintf(stdout,"\n Sent DATA to the next ODR\n");
						}
						else
						{
							//fprintf(stdout,"There is no route to the destination of RREP in the routing table at the source ODR.\n");
							//fprintf(stdout,"\nParking the DATA.\n");
							insert_into_store_Data_RREP(odrecv_msg.srcCanonicalIP,odrecv_msg.destCanonicalIP,odr_msg);   

							odrmsgtyp=0;
                            broadcastID=1;

                            //fprintf(stdout,"Sending the RREQ again.\n");
                            fill_ODR_message(&odr_msg,odrmsgtyp,0,hopcount,++broadcastID,srcflag,canonicalIP,0,odrecv_msg.destCanonicalIP,0,"Nothing");
                            
							fprintf(stdout,"\nODR at node %s : Sending frame header, Source %s and Destination %s\n",get_hostname(canonicalIP),get_hostname(canonicalIP),"ff:ff:ff:ff:ff:ff");
                            printf("\nODR message: Type %d, Source: %s ,",odrmsgtyp,get_hostname(canonicalIP));
                            printf("Destination:%s\n",get_hostname(odrecv_msg.destCanonicalIP));
							broadcast_to_ODR(structlen,odr_msg,0);		// Park the data and send RREQ
						}	
					} 

				} // ODR_Message type=2 end
			}// FD_ISSET for PF packet end
		} // select PF packet loop end
			
	}// infinite foor loop end
}// main end

void insert_wellknown_path(int port,char sunpath[])
{
	//fprintf(stdout,"PORT:%d\n",port);
	//fprintf(stdout,"SUNPATH:%s\n",sunpath);
	struct wellknown_path *node=(struct wellknown_path*)malloc(sizeof(struct wellknown_path));
	if(head==NULL)
	{
		//fprintf(stdout,"Head is NULL.\n");
		node->port=port;
		strncpy(node->sun_path_file,sunpath,strlen(sunpath));
		node->next=NULL;
		head=node;
		curr=node;
	}
	else
	{	
	       //fprintf(stdout,"Head is not NULL.\n");
		node->port=port;
		strncpy(node->sun_path_file,sunpath,strlen(sunpath));
		curr->next=node;
		node->next=NULL;
		curr=node;
	}
}

void print_wellknown_path()
{
	fprintf(stdout,"\nPort number and Well-known pathnames:\n");
	struct wellknown_path *ptr=head;
	while(ptr)
	{
		fprintf(stdout,"The port number:%d\n",ptr->port);
		fprintf(stdout,"The well known pathname:%s\n",ptr->sun_path_file);
		ptr=ptr->next;
	}
}
char* get_pathname(int recvport)
{
	//fprintf(stdout,"RECVPORT:%d\n",recvport);
	struct wellknown_path *ptr=head;
	char filename[MAXLINE];
	memset(&filename[0], 0, sizeof(filename));
	//strcpy(filename,"");
	while(ptr)
	{
		if(ptr->port==recvport)
		{
			//fprintf(stdout,"IN GET PATH NAME: PORT :%d",ptr->port);
			strncpy(filename,ptr->sun_path_file,strlen(ptr->sun_path_file));
			filename[strlen(ptr->sun_path_file)] = 0;
			break;
		}
		ptr=ptr->next;
	}
	//fprintf(stdout,"In get pathname:%s\n",filename);
 	return filename;
}
int insert_port_num(char* sunpath)
{
	struct wellknown_path *ptr=head;
	int flag=0;
	int sport;
	while(ptr)
	{
		if((strcmp(ptr->sun_path_file,sunpath))==0)
		{
			flag=1;
			sport=ptr->port;
			break;
		}
		ptr=ptr->next;
	}
	if(flag==0)
	{
		//fprintf(stdout,"Client port:%d\n",cliport);
		//fprintf(stdout,"Client sunpath:%s\n",sunpath);
		sport=cliport++;
		insert_wellknown_path(sport,sunpath);
	}
	
	return sport;
}		
						
void broadcast_to_ODR(int structlen,struct ODR_message odrm,int ifindex)
{
	//fprintf(stdout,"\nIn broadcast_to_ODR function.\n");	
	int i=0,j=0;

	struct sockaddr_ll ethaddr;
	
	void* buffer =(void*) malloc(ETH_FRAME_LEN); // ethernet frame
	
	unsigned char* etherhead= buffer;
	
	unsigned char* data= buffer+14; // payload		

	struct ethhdr *eh= (struct ethhdr *)etherhead; // pointer to the ethernet header

	unsigned char src_mac[6];
	unsigned int iMac[6];
	char buf[MAXLINE];
	char temp[10];
	unsigned char dest_mac[6]={0xff,0xff,0xff,0xff,0xff,0xff};
	memset(&temp[0], 0, sizeof(temp));
	//strcpy(temp,"");
	strcpy(temp,"0x");
	memset(&buf[0], 0, sizeof(buf));
	//strcpy(buf,"");
	for(i=0;i<structlen;i++)
	{	
		if(ha_info[i].hw_index==ifindex) // Do not broadcast from the same interface on which the RREQ was received
		{
			continue;
		}
		
		sscanf(ha_info[i].hw_addr, "%x:%x:%x:%x:%x:%x", &iMac[0], &iMac[1], &iMac[2], &iMac[3], &iMac[4], &iMac[5]);
		for(j=0;j<6;j++)
		{
			src_mac[j]= (unsigned char) iMac[j];
			//fprintf(stdout,"%x\n",src_mac[j]);
		}
		
		ethaddr.sll_family=PF_PACKET;
		ethaddr.sll_protocol=htons(USID_PROTO);
		ethaddr.sll_ifindex=ha_info[i].hw_index;
		
		ethaddr.sll_halen=ETH_ALEN;
		ethaddr.sll_addr[0]= 0xff;
		ethaddr.sll_addr[1]= 0xff;
		ethaddr.sll_addr[2]= 0xff;
		ethaddr.sll_addr[3]= 0xff;
		ethaddr.sll_addr[4]= 0xff;
		ethaddr.sll_addr[5]= 0xff;
		ethaddr.sll_addr[6]= 0x00;
		ethaddr.sll_addr[7]= 0x00;

                eh->h_proto= htons(USID_PROTO); // protocol type in the ethernet header

		memcpy((void*) buffer, (void*)dest_mac,ETH_ALEN);
		memcpy((void*)(buffer+ETH_ALEN),(void*)src_mac,ETH_ALEN);
		
		memcpy((void*) data, (void*) &odrm,sizeof(struct ODR_message)); //ODR_message to be sent as PAYLOAD in Ethernet frame

		if((sendto(ha_info[i].pfsockfd,buffer,ETH_FRAME_LEN,0,(SA*)&ethaddr,sizeof(ethaddr)))<0)
			err_sys("ODR send to error.");
		//fprintf(stdout,"\nBroadcasted the RREQ.\n");
	}
}

void fill_ODR_message(struct ODR_message *odrm,int typ,int rrep_flag,int hpcnt,int bID,int forceReDesc,char* srcCanIP,int srcport,char* destCanIP,int destport,char* destmsg)
{

	//fprintf(stdout,"\n......In fill_ODR_message.......\n");
	odrm->type=typ;
	odrm->rrep_sent_flag=rrep_flag; 
	odrm->hopcount=hpcnt; 
	odrm->broadcast_ID=bID; 
	odrm->force_redisc=forceReDesc;
	strcpy(odrm->srcCanonicalIP,srcCanIP); 
	odrm->src_port=srcport;
	strcpy(odrm->destCanonicalIP,destCanIP); 
	odrm->dest_port=destport;
	odrm->appBytes=strlen(destmsg);  
	//if(destmsg==NULL)
	//	strcpy(odrm->data,"");
	strcpy(odrm->data,destmsg);

}


int traverse_route_table(char* destCanIP,int* outInt,char* nextMac,int* routeBID,int* rhopcnt,int staleness)
{
	//fprintf(stdout,"\n--------In traverse_route_table function--------\n");
	struct route_table *ptr=rhead;

	while(ptr)
	{
		if(strcmp(ptr->destCanonicalIP,destCanIP)==0)
		{
			if((difftime(time(NULL),ptr->timestamp))>staleness)
			{
				fprintf(stdout,"\nThe route to %s is stale.",ptr->destCanonicalIP);
				delete_routing_table(ptr->destCanonicalIP,ptr->broadcastID);
				return 0;
			}
			else
			{
				*outInt=ptr->out_interface;
				strcpy(nextMac,ptr->nextHopMacAddr);
				*routeBID=ptr->broadcastID;
				*rhopcnt=ptr->hop_count;
				return 1;
			}
		}
		else
		{
			ptr=ptr->next;
		}
	}
	return 0;

}

void send_to_ODR(int structlen,struct ODR_message odrm,int out_index,char* nextHopMac)
{
        //fprintf(stdout,"\nIn send_to_ODR function.\n");
		//fprintf(stdout,"\nNextHopMac:%s\n",nextHopMac);
        int i=0,j=0;

        struct sockaddr_ll ethaddr;

        void* buffer =(void*) malloc(ETH_FRAME_LEN); // ethernet frame

        unsigned char* etherhead= buffer;

        unsigned char* data= buffer+14; // payload              

        struct ethhdr *eh= (struct ethhdr *)etherhead; // pointer to the ethernet header

        unsigned char src_mac[6];
        unsigned int iMac[6];
        char srcbuf[19],destbuf[19];
        unsigned char dest_mac[6];
	
		memset(&srcbuf[0], 0, 18);
		memset(&destbuf[0], 0, 18);
        //strcpy(srcbuf,"");
        //strcpy(destbuf,"");
		
		for(i=0;i<structlen;i++)
		{
			if(ha_info[i].hw_index==out_index)
			{		
				strncpy(srcbuf,ha_info[i].hw_addr, strlen(ha_info[i].hw_addr));
				break;
			}
		}
			//Ashish***: srcbuf length is 18, index 18 is out of bound here.
        	srcbuf[strlen(ha_info[i].hw_addr)]='\0';
		// To get the source ODR MAC address
                sscanf(srcbuf, "%x:%x:%x:%x:%x:%x", &iMac[0], &iMac[1], &iMac[2], &iMac[3], &iMac[4], &iMac[5]);
                for(j=0;j<6;j++)
                {
                        src_mac[j]= (unsigned char) iMac[j];
                        //fprintf(stdout,"%x\n",src_mac[j]);
                }
		
		// To get the destination ODR MAC address
		sscanf(nextHopMac, "%x:%x:%x:%x:%x:%x", &iMac[0], &iMac[1], &iMac[2], &iMac[3], &iMac[4], &iMac[5]);
                for(j=0;j<6;j++)
                {
                        dest_mac[j]= (unsigned char) iMac[j];
                        //fprintf(stdout,"%x\n",src_mac[j]);
                }

                ethaddr.sll_family=PF_PACKET;
                ethaddr.sll_protocol=htons(USID_PROTO);
                ethaddr.sll_ifindex=out_index;

                ethaddr.sll_halen=ETH_ALEN;
                ethaddr.sll_addr[0]= dest_mac[0];
                ethaddr.sll_addr[1]= dest_mac[1];
				ethaddr.sll_addr[2]= dest_mac[2];
                ethaddr.sll_addr[3]= dest_mac[3];
                ethaddr.sll_addr[4]= dest_mac[4];
                ethaddr.sll_addr[5]= dest_mac[5];
                ethaddr.sll_addr[6]= 0x00;
                ethaddr.sll_addr[7]= 0x00;

                eh->h_proto= htons(USID_PROTO); // protocol type in the ethernet header

                memcpy((void*) buffer, (void*)dest_mac,ETH_ALEN);
                memcpy((void*)(buffer+ETH_ALEN),(void*)src_mac,ETH_ALEN);

                memcpy((void*) data, (void*) &odrm,sizeof(struct ODR_message)); //ODR_message to be sent as PAYLOAD in Ethernet frame

                if((sendto(ha_info[i].pfsockfd,buffer,ETH_FRAME_LEN,0,(SA*)&ethaddr,sizeof(ethaddr)))<0)
                        err_sys("ODR send to error.");
                //fprintf(stdout,"\nSent to ODR with MAC address:%s\n",nextHopMac);
	
}

void insertinto_route_table(char* destCanIP,int outInt,char* srcMac,int hopcnt,time_t timestmp,int bID)
{
	
	fprintf(stdout,"\nInserting route for: %s\n",destCanIP);

	struct route_table *node=(struct route_table*)malloc(sizeof(struct route_table));
        if(rhead==NULL)
        {
           //fprintf(stdout,"Head is NULL.\n");
			strcpy(node->destCanonicalIP,destCanIP);
        	node->out_interface=outInt;
        	strcpy(node->nextHopMacAddr,srcMac);
        	node->hop_count=hopcnt;
        	node->timestamp=timestmp;
			node->broadcastID=bID;
			node->next=NULL;
            rhead=node;
           	rcurr=node;
        }
        else
        {
			strncpy(node->destCanonicalIP,destCanIP,strlen(destCanIP));
            node->out_interface=outInt;
            strncpy(node->nextHopMacAddr,srcMac,strlen(srcMac));
            node->hop_count=hopcnt;
			//strcpy(node->timestamp,"");
           	node->timestamp=timestmp;
			node->broadcastID=bID;
            rcurr->next=node;
            node->next=NULL;
            rcurr=node;
        }
}


void update_route_table(char* srcCanIP,int index,char* srcmac,int hopcnt,int bID,time_t tstamp)
{
	//fprintf(stdout,"\nUpdating the Routing table.\n");
	struct route_table *ptr=rhead;
	while(ptr)
	{
		if((strcmp(ptr->destCanonicalIP,srcCanIP)==0) && (ptr->broadcastID==bID))
		{
			ptr->out_interface=index;
			strcpy(ptr->nextHopMacAddr,srcmac);
		 	ptr->hop_count=hopcnt;
			ptr->timestamp=tstamp;
		}
		ptr=ptr->next;
	}

}
void print_routing_table()
{
        fprintf(stdout,"\n==========Routing table==========\n");
        struct route_table *ptr=rhead;
        while(ptr)
        {
			fprintf(stdout,"Destination Canonical IP:%s\n",ptr->destCanonicalIP);
            fprintf(stdout,"Outgoing Interface:%d\n",ptr->out_interface);
			fprintf(stdout,"Next Hop MAC Address:%s\n",ptr->nextHopMacAddr);
			fprintf(stdout,"Hop Count:%d\n",ptr->hop_count);
			fprintf(stdout,"Timestamp:%s\n",ctime(&(ptr->timestamp)));
            ptr=ptr->next;
        }
}		 

void insert_into_store_Data_RREP(char* srcaddr,char* destaddr,struct ODR_message odrmsg)
{
	struct store_Data_RREP *node=(struct store_Data_RREP*) malloc(sizeof(struct store_Data_RREP));
	
    if(shead==NULL)
        {
           //fprintf(stdout,"Head is NULL.\n");
            strcpy(node->srcCanonicalIP,srcaddr);
            strcpy(node->destCanonicalIP,destaddr);
            node->odrm=odrmsg;
            node->next=NULL;
            shead=node;
            scurr=node;
        }
        else
        {
            strcpy(node->srcCanonicalIP,srcaddr);
            strcpy(node->destCanonicalIP,destaddr);
            node->odrm=odrmsg;
            scurr->next=node;
            node->next=NULL;
            scurr=node;
        }
}

/*void traverse_store_Data_RREP(char* destaddr)
{
	struct store_Data_RREP *ptr=shead;
	
	while(ptr)
    {
        if(strcmp(ptr->destCanonicalIP,destaddr)==0)
        {
            send_to_ODR;
        }
	}
}*/

void delete_store_Data_RREP(char* destaddr)
{
	//fprintf(stdout,"\nDeleting stored data.");
	//fprintf(stdout,"\n DEST CAN IP:%s\n",shead->destCanonicalIP);
	//struct store_Data_RREP *ptr=shead;
	//struct store_Data_RREP *prev=shead;
	struct store_Data_RREP *temp;

	if (strcmp(shead->destCanonicalIP,destaddr)==0) {
		temp = shead->next;
		free(shead);
		shead = temp;
	}
	else {
		struct store_Data_RREP *start = shead;
		while (start->next) {
			if (strcmp(start->next->destCanonicalIP,destaddr)==0) {
				temp = start->next->next;
				free(start->next);
				
				start->next = temp;
				break;
			}	
			if (start->next)
				start = start->next;
		}
	}

	
	//fprintf(stdout,"\nDeleting stored data.");
	/*while(ptr)
	{
		if(strcmp(ptr->destCanonicalIP,destaddr)==0)
		{
			//Ashish***: What is the logic here??? Why compare ptr again with shead?? and why make shead null?? and free temp??
			if(ptr==shead)
			{	
				if(shead->next==NULL)
				{
					//temp=shead;
					free(shead);
					shead=NULL;
					//free(temp);
					return;
				}
				else
				{
					temp=shead->next;
					free(shead);
					shead= temp;
					return;
				}
			}
			else
				break;
		}
		else
		{
			ptr=ptr->next;
		}
	}
	
	while(prev->next!=NULL && prev->next!=ptr)
		prev=prev->next;
	
	prev->next=prev->next->next;
	
	free(ptr);*/
}
	
void print_store_Data_RREP()
{

        fprintf(stdout,"\n==========STORED DATA/RREP table==========\n");
        struct store_Data_RREP *ptr=shead;
        while(ptr)
        {
            fprintf(stdout,"Source Canonical IP:%s\n",ptr->srcCanonicalIP);
            fprintf(stdout,"Destination Canonical IP:%s\n",ptr->destCanonicalIP);
            ptr=ptr->next;
        }
}
			
void delete_routing_table(char* destCanIP,int bID)
{
	fprintf(stdout,"\nDeleting the stale route entry.");
    //struct route_table *ptr=rhead;
    //struct route_table *prev=rhead;
    struct route_table *temp;

	if ((strcmp(rhead->destCanonicalIP,destCanIP)==0) && (rhead->broadcastID==bID) ) {
        temp = rhead->next;
        free(rhead);
        rhead = temp;
    }
    else {
        struct route_table *start = rhead;
        while (start->next) {
            if (strcmp(start->next->destCanonicalIP,destCanIP)==0) {
                temp = start->next->next;
                free(start->next);

                start->next = temp;
                break;
            }
            if (start->next)
                start = start->next;
        }
    }

    /*while(ptr)
    {
        if((strcmp(ptr->destCanonicalIP,destCanIP)==0) && (ptr->broadcastID==bID))
        {	// Ashish***: Same as above??
            if(ptr==rhead)
            {
                if(rhead->next==NULL)
                {
                    temp=rhead;
                    rhead=NULL;
                    free(temp);
                    return;
                }
                else
                {
                    temp=rhead;
                    rhead=ptr->next;
                    free(temp);
                    return;
                }
            }
            else
                break;
        }
        else
		{
            ptr=ptr->next;
        }
    }

    while(prev->next!=NULL && prev->next!=ptr)
        prev=prev->next;

    prev->next=prev->next->next;

    free(ptr);*/
}

char* get_hostname(char* destCanIP) 
{
	struct hostent *htpr;
	struct in_addr addr;
	char hostname[10];
	
	if (inet_aton(destCanIP, &addr)<0) // The argument passed is the IP address of the server.
		err_sys("inet_aton error.");
  
	if( (htpr= gethostbyaddr(&addr,sizeof(struct in_addr),AF_INET))==NULL)
      	err_sys("gethostbyaddress error");
	
	
	return htpr->h_name;
}
