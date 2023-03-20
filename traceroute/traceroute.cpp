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
#include <string>
#include <iostream>

u_int16_t compute_icmp_checksum (const void *buff, int length)
{
	u_int32_t sum;
	const u_int16_t* ptr = (const u_int16_t*)buff;
	assert (length % 2 == 0);
	for (sum = 0; length > 0; length -= 2)
		sum += *ptr++;
	sum = (sum >> 16) + (sum & 0xffff);
	return (u_int16_t)(~(sum + (sum >> 16)));
}

struct icmp create_header(){
    // printf("creating header now\n");
    struct icmp header;

    header.icmp_type = ICMP_ECHO;
    header.icmp_code = 0;
    header.icmp_hun.ih_idseq.icd_id = 123; //getpi();??
    header.icmp_hun.ih_idseq.icd_seq = 0; //???
    header.icmp_cksum = compute_icmp_checksum ((u_int16_t*)&header, sizeof(header));

    return header;
} 

ssize_t send_packet(const std::string ip_addr, 
                 const int &sockfd, 
                 const struct icmp &header, 
                 const int ttl){

    struct sockaddr_in recipient;
    bzero (&recipient, sizeof(recipient));
    recipient.sin_family = AF_INET;
    inet_pton(AF_INET, ip_addr.c_str(), &recipient.sin_addr);

    setsockopt (sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(int));

    ssize_t bytes_sent = sendto (
                            sockfd,
                            &header,
                            sizeof(header),
                            0,
                            (struct sockaddr*)&recipient,
                            sizeof(recipient)
                            );
    return bytes_sent;
}

int main( int argc, char *argv[])
{   
    if(argc != 2){
        printf("Error: Invalid parameters!\n");
        printf("Usage: %s <dst_addr>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    std::string dest_addr = argv[1];
    std::cout<<"dest_addres: " << dest_addr << "\n";

    int sockfd; // create a raw socket with ICMP protocol 
    if((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0){
        printf("socket() error\n");
        exit(EXIT_FAILURE);
    }

    for( int i=1; i<=2; i++ ){
        int ttl = i; 

        // sent packets -------------------------------------------------------------
        for(int j=0; j<3; j++){

            struct icmp header = create_header();
            ssize_t bytes_send = send_packet(dest_addr, sockfd, header, ttl);

            printf("ICMP packets send %ld\n", bytes_send);
        }

        // // wait for response packets ------------------------------------------------
        // struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
        // int ready = select (sockfd+1, &descriptors, NULL, NULL, &tv);
        // if(ready < 0){
        //     printf("select() error\n");
        // }
        // printf("gotowych pakietów do odebrania: %d\n", ready);

        // //---------------------------------------------------------------------------

        // // receive packets ----------------------------------------------------------
        // for(int j = 0; j < ready; j++){
        //     struct sockaddr_in sender;
        //     socklen_t          sender_len = sizeof(sender);
        //     u_int8_t           buffer [IP_MAXPACKET];

        //     ssize_t packet_len = recvfrom (sockfd, buffer, IP_MAXPACKET, MSG_DONTWAIT, 
        //                         (struct sockaddr*)&sender, &sender_len);
        //     if(packet_len < 0){
        //         printf("recvfrom() error \n");
        //     }

        //     char sender_ip_str[20]; 
		//     inet_ntop(AF_INET, &(sender.sin_addr), sender_ip_str, sizeof(sender_ip_str));
		//     printf ("Received IP packet with ICMP content from: %s\n", sender_ip_str);
        // }
    }
}