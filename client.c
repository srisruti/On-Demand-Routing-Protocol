//1. Prompts the user to choose between the VM machines which runs the server 
//2. Use gethostbyname to get the eth0(canonical) IP address 
//3. In msg_send- print message
//4. msg_recv to get the "time"
// (put a timeout at mesg_recv) inside or outside
//5. when message is received- print recv message
//6. if timeout- print timeout message 
//7. Timeout- retransmit the message to force route rediscovery- print message on the stdout Repeat all steps again
//8. 

#include "unp.h"
#include "hw_addrs.h"

extern struct hwa_info *get_hw_addrs();

#define SERVDG_PATH "/tmp/servfile9215"
#define ODRDG_PATH "/tmp/odrfile9215"
#define PORT 5001

void msg_send(int,char*,int,char*,int);
void msg_recv(int,char*,char*,int*);
char* get_hostname(char*);
int main(int argc,char* argv[])
{
	
	char option[MAXLINE];
    char input[MAXLINE],destaddr[MAXLINE];
	char tempfilebuf[MAXLINE];
	int sockfd,n;
	int op;
	char message[MAXLINE],recvline[MAXLINE];
	int flag;
	socklen_t odrlen;
	strncpy(input,"vm", 2);
	
	struct sockaddr_un cliaddr,odraddr;

	char srcCanonicalIP[16];
	int src_port;
	
	fd_set tset;
	struct timeval tv;
    int fdp1;

	char clientvm[10];
    struct hwa_info *hwa;
    struct sockaddr *sa;
    char ipaddr[16];
	struct hostent *htpr;

	fprintf(stdout,"=========CLIENT STARTED==========\n\n");
	if((sockfd=socket(AF_LOCAL,SOCK_DGRAM,0))<0)
		err_sys(" client socket create error. ");		
	bzero(&cliaddr,sizeof(cliaddr));
	cliaddr.sun_family=AF_LOCAL;
	strncpy(tempfilebuf,"/tmp/tempcli-XXXXXX",19);
	if((mkstemp(tempfilebuf))<0)
		err_sys("msktemp error in the client");
	strncpy(cliaddr.sun_path,tempfilebuf, strlen(tempfilebuf));
	//fprintf(stdout,"The temporary file name of the client: %s\n",tempfilebuf);
	unlink(tempfilebuf);
	if((bind(sockfd,(SA*) &cliaddr,sizeof(cliaddr)))<0)
		err_sys(" client socket bind error. ");


	// To get the Canonical IP address of the client
	if((hwa=get_hw_addrs())==NULL)
        err_sys("client get_hw_addrs error.");
    
	for(;hwa!=NULL;hwa=hwa->hwa_next)
    {
        if(strcmp(hwa->if_name,"eth0")==0)
        {
            sa=hwa->ip_addr;
            strcpy(ipaddr,sock_ntop_host((SA *)sa,sizeof(sa)));
        }
        
    }
    fprintf(stdout,"The Canonical IP address of the client:%s\n\n",ipaddr);
	
	
	strcpy(clientvm,get_hostname(ipaddr));
	//printf("clientvm%s\n", clientvm);
	//fprintf(stdout,"\nThe client VM :%s\n",clientvm);

	for(;;)
	{
		flag=0;
		memset(&option[0], 0, MAXLINE);
		fprintf(stdout,"\nEnter the Virtual Machine of the server to communicate with (Options: 1-10; To Exit:11)\n");
		fscanf(stdin,"%s",option);
		if((op=atoi(option))==0)
		{	
			fprintf(stdout,"\nInvalid Input.Please enter a Virtual Machine number between 1 and 10.\n");
			exit(0);
		}
		else if(op>=1 && op <=10)
		{
			sprintf(input+2,"%d",op);
			if ((htpr=gethostbyname(input)) == NULL)
           		err_sys("gethostbyname error");
           	if(inet_ntop(htpr->h_addrtype,*(htpr->h_addr_list),destaddr,sizeof(destaddr))<0)
      			err_sys(" inet_ntop error"); 
				
			fprintf(stdout,"\nThe client node at %s sending request to the server at %s\n\n",clientvm,input);
			msg_send(sockfd,destaddr,PORT,message,flag);

			tv.tv_sec=5;
        	tv.tv_usec=0;
			FD_ZERO(&tset);
        	fdp1=sockfd+1;
        	FD_SET(sockfd,&tset);	
			
			if((select(fdp1,&tset,NULL,NULL,&tv))<0)
				err_sys("client timer select error. ");
			if(FD_ISSET(sockfd,&tset))
			{
				msg_recv(sockfd,recvline,srcCanonicalIP,&src_port);
				fprintf(stdout,"\nThe client at node %s recevied response from the server at %s: %s \n\n\n",clientvm,input,recvline);
			}
			else
			{
				fprintf(stdout,"\nThe client at node %s: timed out waiting for response from server at %s\n",clientvm,input);
					
				flag=1;
				fprintf(stdout,"\nResending the request to the server at %s with force rediscovery flag set.\n",input);
				msg_send(sockfd,destaddr,PORT,message,flag);
				tv.tv_sec=5;
                tv.tv_usec=0;
					
				if((select(fdp1,&tset,NULL,NULL,&tv))<0)
                  	err_sys("client timer select error. ");
				if(FD_ISSET(sockfd,&tset))
                {
                    msg_recv(sockfd,recvline,srcCanonicalIP,&src_port);
                    fprintf(stdout,"\n\nThe client at node %s recevied response from the server at %s: %s \n\n\n",clientvm,input,recvline);
                }
				else
				{
					fprintf(stdout,"\nThe client at node %s: timed out again waiting for a response from server at %s\n",clientvm,input);
					fprintf(stdout,"\nGiving up..\n");
					continue;
				}
			}
		}
		else if(op==11)
			fprintf(stdout,"\n\n Client Exiting..\n");

		else
			fprintf(stdout,"\n\nInvalid Input.Please enter a Virtual Machine number between 1 and 10.\n");
	}
}

void msg_send(int sockfd,char* destaddr,int port,char* message,int flag)
{
	
	
	struct sockaddr_un odraddr;
	char recvline[MAXLINE];
	int n=0;
        socklen_t odrlen;
	int buflen=0,i=0;

	char msgsend[MAXLINE];
	
			
	bzero(&odraddr,sizeof(odraddr));
	odraddr.sun_family=AF_LOCAL;

	strcpy(odraddr.sun_path,ODRDG_PATH);
	//fprintf(stdout,"\nThe ODR filename:%s\n",odraddr.sun_path);
	memset(&message[0], 0, MAXLINE);
	strcpy(message,"Requesting Time Service");
	sprintf(msgsend,"%d",sockfd);
	buflen=strlen(msgsend);
	sprintf(msgsend+buflen,";%s",destaddr);
	buflen=strlen(msgsend);
	sprintf(msgsend+buflen,";%d",port);
	buflen=strlen(msgsend);
	sprintf(msgsend+buflen,";%s",message);
	buflen=strlen(msgsend);
	sprintf(msgsend+buflen,";%d",flag);

	odrlen=sizeof(struct sockaddr_un);

	Sendto(sockfd,msgsend,MAXLINE,0,(SA *) &odraddr,odrlen);
}



void msg_recv(int sockfd,char *msgrecv,char *srcCanonicalIP,int *port_num)
{
        int n;
        struct sockaddr_un odraddr;
        socklen_t odrlen;
        char *recvtok;
		char recbuf[MAXLINE];
		odrlen=sizeof(odraddr);

		
       	if((n=recvfrom(sockfd,recbuf,MAXLINE,0,(SA *)&odraddr,&odrlen))<0)
				err_sys("server recvfrom error.");
		//printf("Received msg: %s\n", recbuf);
		
        recvtok=strtok(recbuf,";");
        *port_num=atoi(recvtok);
		//printf("%d\n", *port_num);
        recvtok=strtok(NULL,";");
		memset(&msgrecv[0], 0, MAXLINE);
        strncpy(msgrecv,recvtok,strlen(recvtok));
		msgrecv[strlen(recvtok)] = 0;
		//printf("Recbuf: %s\n", msgrecv);
        recvtok=strtok(NULL,";");
		memset(&srcCanonicalIP[0], 0, 16);
        strncpy(srcCanonicalIP,recvtok,strlen(recvtok));
		//printf("%s\n", srcCanonicalIP);
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
