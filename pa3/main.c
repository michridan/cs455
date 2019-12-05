#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/ether.h>

#define BUF_SIZ		65536
#define SEND 0
#define RECV 1

struct message
{
	struct ether_header hdr;
	char buf[1024];
};

struct arp_hdr
{
	uint16_t ar_hrd; // Hardware type, 1 for ethernet
	uint16_t ar_pro; // Protocol type, ETH_P_IP
	unsigned char ar_hln; // HWaddr length (# of octets), 6
	unsigned char ar_pln; // IPaddr length (# of octets), 4
	uint16_t ar_op; // 1 for request, 2 for reply
	unsigned char ar_sha[6]; // Sender HWaddr (hex). In reply, original dest HWaddr
	unsigned char ar_sip[4]; // Sender IP (hex)
	unsigned char ar_tha[6]; // Tarder HWaddr, ignore in request
	unsigned char ar_tip[4]; // Target IP
};

/*
Definition found via StackOverflow:
struct ip {
#if BYTE_ORDER == LITTLE_ENDIAN 
    u_char  ip_hl:4,        // header length
        ip_v:4;         // version 
#endif
#if BYTE_ORDER == BIG_ENDIAN 
    u_char  ip_v:4,         // version 
        ip_hl:4;        // header length 
#endif
    u_char  ip_tos;         // type of service
    short   ip_len;         // total length 
    u_short ip_id;          // identification 
    short   ip_off;         // fragment offset field 
#define IP_DF 0x4000            // dont fragment flag 
#define IP_MF 0x2000            // more fragments flag 
    u_char  ip_ttl;         // time to live 
    u_char  ip_p;           // protocol 
    u_short ip_sum;         // checksum 
    struct  in_addr ip_src,ip_dst;  // source and dest address 
};
*/

struct ip init_ip()
{
	struct ip hdr;

	hdr.ip_v = 04;
	hdr.ip_hl = 05;
	hdr.ip_off = htons(0);
	hdr.ip_ttl = 255;
	hdr.ip_p = 200;
	hdr.ip_id = htons(7);

	return hdr;
}

struct arp_hdr init_arp_req()
{
	struct arp_hdr hdr;

	hdr.ar_hrd = htons(1);
	hdr.ar_pro = htons(ETH_P_IP);
	hdr.ar_hln = 6;
	hdr.ar_pln = 4;
	hdr.ar_op = htons(1);

	return hdr;
}

void send_msg(int sockfd, struct ifreq if_idx, struct message msg)
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

void recv_arp(int sockfd, char *addr)
{
	int recvLen, recved = 0;
    struct sockaddr_ll sk_addr;
    int sk_addr_size = sizeof(struct sockaddr_ll);
	struct message msg;
	struct arp_hdr arp;
	char buf[BUF_SIZ];

	while(recved == 0)
	{
 		memset(&sk_addr, 0, sk_addr_size);
 		recvLen = recvfrom(sockfd, buf, BUF_SIZ, 0, (struct sockaddr *)&sk_addr, &sk_addr_size);
		memcpy(&msg, buf, sizeof(struct message));
		memcpy(&arp, msg.buf, sizeof(struct arp_hdr));
		if(ntohs(arp.ar_op) == 2)
		{
			recved = 1;
			printf("Target Address: %hhx:%hhx:%hhx:%hhx:%hhx:%hhx\n", arp.ar_sha[0], arp.ar_sha[1], arp.ar_sha[2], arp.ar_sha[3], arp.ar_sha[4], arp.ar_sha[5]);
			memcpy(addr, arp.ar_sha, 6);
		}
	}
}

void recv_ip(int sockfd)
{
	int recvLen, recved = 0;
    struct sockaddr_ll sk_addr;
    int sk_addr_size = sizeof(struct sockaddr_ll);
	struct message msg;
	struct ip hdr;
	char buf[BUF_SIZ];

	while(recved == 0)
	{
 		memset(&sk_addr, 0, sk_addr_size);
 		recvLen = recvfrom(sockfd, buf, BUF_SIZ, 0, (struct sockaddr *)&sk_addr, &sk_addr_size);
		memcpy(&msg, buf, sizeof(struct message));
		memcpy(&hdr, msg.buf, sizeof(struct ip));
		if(hdr.ip_p == 200)
		{
			recved = 1;
			printf("Message Received: %s\n", msg.buf + sizeof(struct ip));
		}
	}
}

unsigned int get_ip_saddr(char *if_name, int sockfd)
{
	struct ifreq if_idx;
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, if_name, IFNAMSIZ-1);
	if(ioctl(sockfd, SIOCGIFADDR, &if_idx) < 0)
		perror("SIOCGIFADDR");
	return ((struct sockaddr_in *)&if_idx.ifr_addr)->sin_addr.s_addr;
}

unsigned int get_netmask(char *if_name, int sockfd)
{
	struct ifreq if_idx;
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, if_name, IFNAMSIZ-1);
	if((ioctl(sockfd, SIOCGIFNETMASK, &if_idx)) == -1)
		perror("ioctl():");
	return ((struct sockaddr_in *)&if_idx.ifr_netmask)->sin_addr.s_addr;
}

int16_t ip_checksum(void* vdata, size_t length)
{
	char *data = (char *)vdata;
	uint32_t acc = 0xffff;

	for (size_t i = 0; i + 1 < length; i += 2)
	{
		uint16_t word;
		memcpy(&word, data + i, 2);
		acc += ntohs(word);
		if (acc > 0xffff)
			acc -= 0xffff;
	}

	if (length & 1)
	{
		uint16_t word = 0;
		memcpy(&word, data + length - 1, 1);
		acc += ntohs(word);
		if (acc > 0xffff)
			acc -= 0xffff;
	}
	return htons(~acc);
}

int16_t ip_length(int hl, char *buf)
{
	return htons(strlen(buf) + (4 * hl));
}

int main(int argc, char *argv[])
{
	int mode, sockfd, errno;
	unsigned int netmask;
	char hw_addr[6];
	char interfaceName[IFNAMSIZ];
	char buf[BUF_SIZ];
	struct message msg;
    struct ifreq if_idx;
	struct in_addr destip, routip;
	struct arp_hdr arp = init_arp_req();
	struct ip ip_hdr = init_ip();
	memset(buf, 0, BUF_SIZ);
	
	if (argc >= 3)
	{
		strncpy(interfaceName, argv[2], IFNAMSIZ);
		if (argc == 6)
		{
			inet_aton(argv[3], &destip);
			inet_aton(argv[4], &routip);
			ip_hdr.ip_dst = destip;
			strncpy(buf, argv[5], BUF_SIZ);
			ip_hdr.ip_len = ip_length(ip_hdr.ip_hl, buf);
		}
	}
	else
	{
		fprintf(stderr, "./455_proj3 Send <InterfaceName>  <DestIP> <RouterIP> <Message>\n");
		fprintf(stderr, "./455_proj3 Recv <InterfaceName>\n");
		exit(1);
	}

    // Create Socket
    if((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
    {
        perror("socket() failed\n");
    }

	if(strcmp(argv[1], "Send") == 0)
	{
	    // Assign Socket interface
	    struct ifreq if_idx_s;
	    memset(&if_idx_s, 0, sizeof(struct ifreq));
	    strncpy(if_idx_s.ifr_name, interfaceName, IFNAMSIZ-1);
	    if(ioctl(sockfd, SIOCGIFHWADDR, (char *)&if_idx_s) < 0)
	        perror("SIOCSIFHWADDR");
	    memset(&if_idx, 0, sizeof(struct ifreq));
	    strncpy(if_idx.ifr_name, interfaceName, IFNAMSIZ-1);
	    if(ioctl(sockfd, SIOCGIFINDEX, (char *)&if_idx) < 0)

        perror("SIOCGIFINDEX");
		// Get source IP
		int sip = get_ip_saddr(interfaceName, sockfd);
		memcpy(arp.ar_sip, &sip, 4);
		ip_hdr.ip_src.s_addr = sip;

		// Choose dest IP via netmask comparison
		netmask = get_netmask(interfaceName, sockfd);

		if((destip.s_addr & netmask) == (sip & netmask))
		{
			memcpy(arp.ar_tip, &(destip.s_addr), 4);
		}
		else
			memcpy(arp.ar_tip, &(routip.s_addr), 4);


	    // Get source MAC 
	    memcpy(msg.hdr.ether_shost, if_idx_s.ifr_hwaddr.sa_data, 6);
	    memcpy(arp.ar_sha, if_idx_s.ifr_hwaddr.sa_data, 6);

		// Finish setting up ARP
		memset(msg.hdr.ether_dhost, 255, 6);
		msg.hdr.ether_type = htons(ETH_P_ARP);
		memcpy(msg.buf, &arp, sizeof(struct arp_hdr));

		// Send ARP and wait for response
		send_msg(sockfd, if_idx, msg);
		recv_arp(sockfd, msg.hdr.ether_dhost); // Gets dest mac

		// Compute checksum
		ip_hdr.ip_sum = ip_checksum(&ip_hdr, sizeof(struct ip));

		// Fix ether_type and msg.buf
		msg.hdr.ether_type = htons(ETH_P_IP);
		memcpy(msg.buf, &ip_hdr, sizeof(struct ip));
		memcpy(msg.buf + sizeof(struct ip), buf, BUF_SIZ);

		// Send new packet
		send_msg(sockfd, if_idx, msg);
	}
	else
	{
		recv_ip(sockfd);
	}

	return 0;
}

