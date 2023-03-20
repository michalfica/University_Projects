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
#include <chrono>
#include <ctime> 

void print_as_bytes (unsigned char* buff, ssize_t length)
{
	for (ssize_t i = 0; i < length; i++, buff++)
		printf ("%.2x ", *buff);	
}

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

struct icmp create_header(u_int16_t id, u_int16_t seq){
    struct icmp header;

    header.icmp_type = ICMP_ECHO;
    header.icmp_code = 0;
    header.icmp_hun.ih_idseq.icd_id = id;   //getpid();??
    header.icmp_hun.ih_idseq.icd_seq = seq; //???

    std::cout << "ustawiam id pakietu: " << id << " seq: " << seq <<'\n';
    header.icmp_cksum = 0;
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

    for( int i=1; i<=9; i++ ){
        int ttl = i; 

        // sent packets -------------------------------------------------------------
        for(int j=0; j<3; j++){
            u_int16_t seq = j; //(ttl << 2) + j;
            struct icmp header = create_header(ttl, seq);
            ssize_t bytes_send = send_packet(dest_addr, sockfd, header, ttl);

            printf("ICMP packets send %ld\n", bytes_send);
        }
        std::cout << "wysłałem wszystko dla ttl = " << ttl << '\n';
        // auto start = std::chrono::system_clock::now();
        // // Some computation here
        // auto end = std::chrono::system_clock::now();
        // std::chrono::duration<double> elapsed_seconds = end-start;

        // // wait for response packets ------------------------------------------------
        fd_set descriptors;
        FD_ZERO (&descriptors);
        FD_SET (sockfd, &descriptors);

        struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;

        int responses = 0;

        while (responses < 3 && (tv.tv_sec > 0 || tv.tv_usec > 0))
        {
            std::cout<<"zaczynam czekać (max 1sec) na pojawienie się jakiegoś pakietu\n";
            int ready = select (sockfd+1, &descriptors, NULL, NULL, &tv);
            std::cout<<"gotowych pakietów jest: "<<ready<<'\n';

            if(ready < 0){
                printf("select() error\n");
            } else if (ready == 0){
                break;
            }
            
            struct sockaddr_in 	sender;	
            socklen_t 			sender_len = sizeof(sender);
            u_int8_t 			buffer[IP_MAXPACKET];

            //---------------------------------------------------------------------------
            // receive packets ----------------------------------------------------------
            for (;;) {
                std::cout<<"zaczynam odbieranie pakietów z gniazda receivem\n";

                ssize_t packet_len = recvfrom (sockfd, buffer, IP_MAXPACKET, MSG_DONTWAIT, (struct sockaddr*)&sender, &sender_len);
                std::cout<<"odebralem dl pakietu to: "<<packet_len<<'\n';

                if (packet_len < 0) {
                    fprintf(stderr, "recvfrom error: %s\n", strerror(errno)); 
                    // return EXIT_FAILURE;
                    break;
                }
                if( packet_len == 0){
                    break;
                }

                char sender_ip_str[20]; 
                inet_ntop(AF_INET, &(sender.sin_addr), sender_ip_str, sizeof(sender_ip_str));
                printf ("Received IP packet with ICMP content from: %s\n", sender_ip_str);

                struct ip* 			ip_header = (struct ip*) buffer;
                ssize_t				ip_header_len = 4 * ip_header->ip_hl;
                u_int8_t* icmp_packet = buffer + 4 * ip_header->ip_hl;
                struct icmp* icmp_header = (struct icmp*) icmp_packet;
                ssize_t icmp_header_len = 8;
                
                // Type 8 - loopback
                bool skip = false;
                if(icmp_header->icmp_type == 8) {
                    skip = true;
                }

                // Type 11 - TTL exceeded, extracting the original request
                if(icmp_header->icmp_type == 11) {
                    ssize_t inner_packet = ip_header_len + icmp_header_len; // Moving to inner ICMP
                    ip_header = (struct ip *)(buffer + inner_packet);
                    ip_header_len = 4 * ip_header->ip_hl;
                    icmp_packet = buffer + inner_packet + ip_header_len;
                    icmp_header = (struct icmp *)icmp_packet;
                }

                printf ("IP header: "); 
                print_as_bytes (buffer, ip_header_len);
                printf("\n");

                printf ("IP data:   ");
                print_as_bytes (buffer + ip_header_len, packet_len - ip_header_len);
                printf("\n");

                u_int16_t id  = icmp_header->icmp_hun.ih_idseq.icd_id;
                u_int16_t seq = icmp_header->icmp_hun.ih_idseq.icd_seq;

                std::cout << "id pakietu: " << id << " seq: " << seq << "\n\n";

                // Checking if packet is a response to our last requests
                if(seq == ttl && !skip) {
                    responses++;
                }

            }
            
        }
    }
}