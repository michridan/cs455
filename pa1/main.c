#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>

#define BUF_SIZ		65536
#define SEND 0
#define RECV 1

struct message
{
	struct ether_header hdr;
	char buf[1024];
};

void send_message(int sockfd, struct ifreq if_idx, struct message msg)
{
	int byteSent, sendLen = sizeof(struct message);
    struct sockaddr_ll sk_addr;
    int sk_addr_size = sizeof(struct sockaddr_ll);
	char buf[BUF_SIZ];

    memset(&sk_addr, 0, sk_addr_size);
    sk_addr.sll_ifindex = if_idx.ifr_ifindex;
    sk_addr.sll_halen = ETH_ALEN;
	memcpy(buf, &msg, BUF_SIZ);
    if((byteSent = sendto(sockfd, buf, sendLen, 0, (struct sockaddr*)&sk_addr, sizeof(struct sockaddr_ll))) < 0)
		perror("Send");
	printf("%d of %d bytes sent\n", byteSent, sendLen);
}

void recv_message(int sockfd)
{
	int recvLen;
    struct sockaddr_ll sk_addr;
    int sk_addr_size = sizeof(struct sockaddr_ll);
	struct message msg;
	char buf[BUF_SIZ];

    memset(&sk_addr, 0, sk_addr_size);
    recvLen = recvfrom(sockfd, buf, BUF_SIZ, 0, (struct sockaddr *)&sk_addr, &sk_addr_size);
	memcpy(&msg, buf, sizeof(struct message));
	printf("Message From: %hhx:%hhx:%hhx:%hhx:%hhx:%hhx\n", msg.hdr.ether_shost[0], msg.hdr.ether_shost[1], msg.hdr.ether_shost[2], msg.hdr.ether_shost[3], msg.hdr.ether_shost[4], msg.hdr.ether_shost[5]);
	printf("Message Type: %d\n", ntohs(msg.hdr.ether_type));
	printf("%s\n", msg.buf);
}

int main(int argc, char *argv[])
{
	int mode, sockfd, errno;
	char hw_addr[6];
	char interfaceName[IFNAMSIZ];
	char buf[BUF_SIZ];
	struct message msg;
    struct ifreq if_idx;
	memset(buf, 0, BUF_SIZ);
	
	int correct=0;
	if (argc > 1){
		if(strncmp(argv[1],"Send", 4)==0){
			if (argc == 5){
				mode=SEND; 
				sscanf(argv[3], "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &hw_addr[0], &hw_addr[1], &hw_addr[2], &hw_addr[3], &hw_addr[4], &hw_addr[5]);
				strncpy(msg.buf, argv[4], BUF_SIZ);
				correct=1;
				printf("  buf: %s\n", msg.buf);
			}
		}
		else if(strncmp(argv[1],"Recv", 4)==0){
			if (argc == 3){
				mode=RECV;
				correct=1;
			}
		}
		strncpy(interfaceName, argv[2], IFNAMSIZ);
	 }
	 if(!correct){
		fprintf(stderr, "./455_proj2 Send <InterfaceName>  <DestHWAddr> <Message>\n");
		fprintf(stderr, "./455_proj2 Recv <InterfaceName>\n");
		exit(1);
	 }

    //a: Create Socket
    if((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
    {
        perror("socket() failed\n");
    }

    //b: ether_header
    msg.hdr.ether_type = htons(ETH_P_IP);
    memcpy(msg.hdr.ether_dhost, hw_addr, 6);

    //c: Assign Socket interface
    struct ifreq if_idx_s;
    memset(&if_idx_s, 0, sizeof(struct ifreq));
    strncpy(if_idx_s.ifr_name, interfaceName, IFNAMSIZ-1);
    if(ioctl(sockfd, SIOCGIFHWADDR, (char *)&if_idx_s) < 0)
        perror("SIOCSIFHWADDR");
    memset(&if_idx, 0, sizeof(struct ifreq));
    strncpy(if_idx.ifr_name, interfaceName, IFNAMSIZ-1);
    if(ioctl(sockfd, SIOCGIFINDEX, (char *)&if_idx) < 0)
        perror("SIOCGIFINDEX");

    //d: Get source MAC 

    memcpy(msg.hdr.ether_shost, if_idx_s.ifr_hwaddr.sa_data, 6);
    //e: sockaddr_ll
    struct sockaddr_ll sk_addr;
    int sk_addr_size = sizeof(struct sockaddr_ll);

	if(mode == SEND){
		send_message(sockfd, if_idx, msg);
	}
	else if (mode == RECV){
		recv_message(sockfd);
	}

	return 0;
}

