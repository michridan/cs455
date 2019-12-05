#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define SERVER_PORT 5432
// Increased MAX_LINE to add sequence number
#define MAX_LINE 81
#define WIN_SIZE 10

int main(int argc, char * argv[])
{
    FILE *fp;
    struct hostent *hp;
    struct sockaddr_in sin;
    char *host;
    char *fname;
    char buf[MAX_LINE];
    char window[WIN_SIZE][MAX_LINE];
    char last_ack = 0;
    char sn = 0;
    int s;
    int slen;
    int i;
    int file_read = 0;
    // initialize timer value
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000;

    if (argc==3) {
        host = argv[1];
        fname= argv[2];
    }
    else {
        fprintf(stderr, "Usage: ./client_udp host filename\n");
        exit(1);
    }
    /* translate host name into peer’s IP address */
    hp = gethostbyname(host);
    if (!hp) {
        fprintf(stderr, "Unknown host: %s\n", host);
        exit(1);
    }

    fp = fopen(fname, "r");
    if (fp==NULL){
        fprintf(stderr, "Can't open file: %s\n", fname);
        exit(1);
    }

    /* build address data structure */
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
    sin.sin_port = htons(SERVER_PORT);

    /* active open */
    if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket");
        exit(1);
    }

    socklen_t sock_len= sizeof sin;

    // set up recv timer
    if(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("Timer");
        exit(1);
    }
    
    /* main loop: get and send lines of text */
    while(!file_read || sn != last_ack){
        
        if(!file_read && fgets(buf, 80, fp) != NULL)
        {
            sn++;

            // turn buf into useable string
            slen = strlen(buf);
            buf[slen] ='\0';

            // add sn to front of packet
            window[sn % 10][0] = sn;
            window[sn % 10][1] = '\0';

            // add buf to packet
            strcat(window[sn % 10], buf);
        }
        else
        {
            file_read++;
        }

        while((sn == last_ack + 10) || (file_read && sn != last_ack))
        {
            slen = 0;
            // send everything
            for(i = last_ack; i <= sn; i++)
            {
                if(sendto(s, window[i % 10], strlen(window[i % 10]) + 1, 0, (struct sockaddr *)&sin, sock_len)<0){
                    perror("SendTo Error\n");
                    exit(1);
                }

                // Wait for an ack
                if(recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&sin, &sock_len)>0)
                {
                    // only enters if an ack is recieved, otherwise loop repeats
                    last_ack = buf[0];
                }           
            }
        }
    }
    *buf = 0x02;    
        if(sendto(s, buf, 1, 0, (struct sockaddr *)&sin, sock_len)<0){
        perror("SendTo Error\n");
        exit(1);
    }
    fclose(fp);
}

