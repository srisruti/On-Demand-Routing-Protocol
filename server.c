//1. Create unix domain socket
//2. Well Known path name- well known port number
//3. Infinite loop to send and recv messages from-to the client
//4. When it recvs- print message

#include "unp.h"
#include "hw_addrs.h"

extern struct hwa_info *get_hw_addrs();

#define SERVDG_PATH "/tmp/servfile9215"
#define ODRDG_PATH "/tmp/odrfile9215"

void msg_send(int,char*,int,char*,int);
void msg_recv(int,char*,char*,int*);
char* get_hostname(char*);

int main(int argc,char* argv[])
{

	int sockfd;
	char msgrecv[MAXLINE];
	struct sockaddr_un servaddr,cliaddr;
	int n;
	socklen_t clilen;
	char buff[MAXLINE];
	time_t ticks;
	char srcCanonicalIP[16];
	int flag=0;
	int src_port;

	char servvm[10];
    struct hwa_info *hwa;
    struct sockaddr *sa;
    char ipaddr[16];
	
	fprintf(stdout,"===========SERVER STARTED==========\n\n");
	if((sockfd=socket(AF_LOCAL,SOCK_DGRAM,0))<0)
		err_sys(" create server socket error.");
	
	if((hwa=get_hw_addrs())==NULL)
        err_sys("server get_hw_addrs error.");

    for(;hwa!=NULL;hwa=hwa->hwa_next)
    {
        if(strcmp(hwa->if_name,"eth0")==0)
        {
            sa=hwa->ip_addr;
            strcpy(ipaddr,sock_ntop_host((SA *)sa,sizeof(sa)));
        }

    }
    fprintf(stdout,"The Canonical IP address of the server:%s\n",ipaddr);


    strcpy(servvm,get_hostname(ipaddr));
	
	unlink(SERVDG_PATH);
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sun_family=AF_LOCAL;
	strcpy(servaddr.sun_path,SERVDG_PATH);

	if((bind(sockfd,(SA *) &servaddr,sizeof(servaddr)))<0)
		err_sys("server socket bind error");
	
	memset(&buff[0], 0, MAXLINE);
	
	for(;;)
	{
		
		ticks=time(NULL);
		msg_recv(sockfd,msgrecv,srcCanonicalIP,&src_port);
		snprintf(buff, MAXLINE, "%.24s", ctime(&ticks));

		fprintf(stdout,"\nThe server at node %s sending response to client at %s\n",servvm,get_hostname(srcCanonicalIP));
		msg_send(sockfd,srcCanonicalIP,src_port,buff,0);
		
	}
	
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
	//printf("Received from client: %s\n", msgrecv);
	recvtok=strtok(recbuf,";");
	*port_num=atoi(recvtok);
	recvtok=strtok(NULL,";");
	memset(&msgrecv[0], 0, MAXLINE);
	strncpy(msgrecv,recvtok,strlen(recvtok));
	msgrecv[strlen(recvtok)]=0;
	//printf("%s, %s,", msgrecv,recvtok);
	recvtok=strtok(NULL,";");
	memset(&srcCanonicalIP[0], 0, 16);
	strncpy(srcCanonicalIP,recvtok,strlen(recvtok));
	//printf("%s\n", srcCanonicalIP);
}
	
	

void msg_send(int sockfd,char *destaddr,int port_num,char *msg,int flag)
{
	//fprintf(stdout,"The message to be sent to the source:%s\n",msg);
	struct sockaddr_un odraddr;
        char recvline[MAXLINE];
        int n=0;
        socklen_t odrlen;
        int buflen=0,i=0;
        int hwi=0;
        char msgsend[MAXLINE];
        bzero(&odraddr,sizeof(odraddr));
        odraddr.sun_family=AF_LOCAL;
		memset(&odraddr.sun_path[0], 0, sizeof(odraddr.sun_path));
		//strcpy(odraddr.sun_path,"");
        strcpy(odraddr.sun_path,ODRDG_PATH);
        //fprintf(stdout,"\nThe ODR filename:%s\n",odraddr.sun_path);
        sprintf(msgsend,"%d",sockfd);
        buflen=strlen(msgsend);
        sprintf(msgsend+buflen,";%s",destaddr);
        buflen=strlen(msgsend);
        sprintf(msgsend+buflen,";%d",port_num);
        buflen=strlen(msgsend);
        sprintf(msgsend+buflen,";%s",msg);
        buflen=strlen(msgsend);
        sprintf(msgsend+buflen,";%d",flag);
		msgsend[strlen(msgsend)] =0;
		//printf("Individual message: %d;%s;%d;%s;%d...\n", sockfd, destaddr, port_num, msg, flag);
		//printf("Message to send w/o timestamp: %s\n", msgsend);

       // fprintf(stdout,"Message to sent to ODR: %s\n",msgsend);

        odrlen=sizeof(struct sockaddr_un);

       // sendto(sockfd,msgsend,MAXLINE,0,(SA *) &odraddr,odrlen);
	Sendto(sockfd,msgsend,MAXLINE,0,(SA *) &odraddr,odrlen);
       // fprintf(stdout,"\nSent message to the odr: %s\n",msgsend);
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
