#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <assert.h>

u_int16_t compute_icmp_checksum (const void *buff, int length)
{
	u_int32_t sum;
	const u_int16_t* ptr = buff;
	assert (length % 2 == 0);
	for (sum = 0; length > 0; length -= 2)
		sum += *ptr++;
	sum = (sum >> 16) + (sum & 0xffff);
	return (u_int16_t)(~(sum + (sum >> 16)));
}

struct icmp create_header(){
    // printf("creating header now\n");
    struct icmp header

    header.icmp_type = ICMP_ECHO;
    header.icmp_code = 0;
    header.icmp_hun.ih_idseq.icd_id = 123; //getpi();??
    header.icmp_hun.ih_idseq.icd_seq = 0; //???
    header.icmp_cksum = compute_icmp_checksum ((u_int16_t*)&header, sizeof(header));

    return header;
} 

int main( int argc, char *argv[])
{   
    if(argc != 2){
        printf("Error: Invalid parameters!\n");
        printf("Usage: %s <dst_addr>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *dest_addr = argv[1];
    printf("dest_addres: %s\n", dest_addr);

    // create a raw socket with ICMP protocol 
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if(sockfd < 0){
        printf("socket() error\n");
        exit(EXIT_FAILURE);
    }
    printf("socket created \n");

    struct sockaddr_in recipient;
    bzero (&recipient, sizeof(recipient));
    recipient.sin_family = AF_INET;
    inet_pton(AF_INET, dest_addr, &recipient.sin_addr);

    fd_set descriptors;
    FD_ZERO (&descriptors);
    FD_SET (sockfd, &descriptors);

    for( int i=1; i<=30; i++ ){
        int ttl = i;
        setsockopt (sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(int));

        for(int j=0; j<3; j++){
            struct icmp header = create_header();
        }
        // create packet to send ----------------------------------------------------
        //---------------------------------------------------------------------------

        // sent packets -------------------------------------------------------------
        if(sendto(sockfd, &header1, sizeof(header1), 0, 
                    (struct sockaddr*)&recipient, sizeof(recipient)) < 0){
            printf("sendto() header1 error\n");
        }

        if(sendto(sockfd, &header2, sizeof(header2), 0, 
                    (struct sockaddr*)&recipient, sizeof(recipient)) < 0){
            printf("sendto() header2 error\n");
        }

        if(sendto(sockfd, &header3, sizeof(header3), 0, 
                    (struct sockaddr*)&recipient, sizeof(recipient)) < 0){
            printf("sendto() header2 error \n");
        }
        // printf("ICMP packets send\n");
        //---------------------------------------------------------------------------
        
        // wait for response packets ------------------------------------------------
        struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
        int ready = select (sockfd+1, &descriptors, NULL, NULL, &tv);
        if(ready < 0){
            printf("select() error\n");
        }
        printf("gotowych pakietów do odebrania: %d\n", ready);

        //---------------------------------------------------------------------------

        // receive packets ----------------------------------------------------------
        for(int j = 0; j < ready; j++){
            struct sockaddr_in sender;
            socklen_t          sender_len = sizeof(sender);
            u_int8_t           buffer [IP_MAXPACKET];

            ssize_t packet_len = recvfrom (sockfd, buffer, IP_MAXPACKET, MSG_DONTWAIT, 
                                (struct sockaddr*)&sender, &sender_len);
            if(packet_len < 0){
                printf("recvfrom() error \n");
            }

            char sender_ip_str[20]; 
		    inet_ntop(AF_INET, &(sender.sin_addr), sender_ip_str, sizeof(sender_ip_str));
		    printf ("Received IP packet with ICMP content from: %s\n", sender_ip_str);
        }
    }
}