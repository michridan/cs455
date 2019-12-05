#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <fcntl.h>

#define SERVER_PORT 5432
#define MAX_LINE 256

int main(int argc, char * argv[])
{
    char *fname;
    char buf[MAX_LINE];
    struct sockaddr_in sin;
    int len;
    int s, i;
    struct timeval tv;
    tv.tv_sec = 20; // need a failsafe for if it misses the terminator
    tv.tv_usec = 0;
    char sn = 0; 
    FILE *fp;

    if (argc==2) {
        fname = argv[1];
    }
    else {
        fprintf(stderr, "usage: ./server_udp filename\n");
        exit(1);
    }


    /* build address data structure */
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(SERVER_PORT);

    /* setup passive open */
    if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("simplex-talk: socket");
        exit(1);
    }
    if ((bind(s, (struct sockaddr *)&sin, sizeof(sin))) < 0) {
        perror("simplex-talk: bind");
        exit(1);
    }

    socklen_t sock_len = sizeof sin;

    fp = fopen(fname, "w");
    if (fp==NULL){
        printf("Can't open file\n");
        exit(1);
    }

    // set up recv timer
    if(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        perror("Timer");
        exit(1);
    }
    
    while(1){
        len = 0;
        len = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&sin, &sock_len);
        if(len == -1){
                perror("PError");
                break;
        }    
        else if(len == 1){
            if (buf[0] == 0x02){
                printf("Transmission Complete\n");
                break;
            }
            else{
                perror("Error: Short packet\n");
            }
        }    
        else if(len > 1){
            if(buf[0] > sn + 1 || buf[0] == sn) // meaning we have the wrong line
            {
                // Send ack of our old sn to tell client to resend
                if(sendto(s, &sn, 1, 0, (struct sockaddr *)&sin, sock_len)<0){
                perror("SendTo Error\n");
                exit(1);
                }
            }
            else if(buf[0] == sn + 1) // meaning we have the correct line
            {
                // Update sn and ack
                sn++;
                if(sendto(s, &sn, 1, 0, (struct sockaddr *)&sin, sock_len)<0){
                perror("SendTo Error\n");
                exit(1);
                }

                // print message (everything after 1st byte)
                if(fputs((char *) buf + 1, fp) < 1){
                    printf("fputs() error\n");
                }
            }
        }
        else
        {
            fprintf(stderr, "connection timed out\n");
            break;
        }

    }
    fclose(fp);
    close(s);
}
