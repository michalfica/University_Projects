#include <arpa/inet.h>
#include <assert.h>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

const int MX_REP = 100;

struct packet {
  int id;
  int ttl;
  int reply_received;
  std::chrono::system_clock::time_point time_start;
  std::chrono::system_clock::time_point time_end;
  std::chrono::duration<double> time_duration;
  std::string reply_ip_addr;
};

int cnt = 0;
packet packets[MX_REP + 5];

void print_reply_info(packet *p1, packet *p2, packet *p3) {
  int row_number = cnt / 3;
  std::cout << row_number << ". ";

  int amount_of_responses =
      p1->reply_received + p2->reply_received + p3->reply_received;
  if (amount_of_responses == 0) {
    std::cout << "*\n";
    return;
  }

  std::set<std::string> distinc_ip_addr;
  distinc_ip_addr.clear();

  if (p1->reply_received == 1) {
    distinc_ip_addr.insert(p1->reply_ip_addr);
  }
  if (p2->reply_received == 1) {
    distinc_ip_addr.insert(p2->reply_ip_addr);
  }
  if (p3->reply_received == 1) {
    distinc_ip_addr.insert(p3->reply_ip_addr);
  }

  for (auto s : distinc_ip_addr)
    std::cout << s << ' ';

  if (amount_of_responses != 3) {
    std::cout << "???\n";
    return;
  }
  double avg_time_response =
      (p1->time_duration.count() + p2->time_duration.count() +
       p3->time_duration.count()) /
      3;
  std::cout << round(avg_time_response * 1000) << "ms \n";
}

u_int16_t compute_icmp_checksum(const void *buff, int length) {
  u_int32_t sum;
  const u_int16_t *ptr = (const u_int16_t *)buff;
  assert(length % 2 == 0);
  for (sum = 0; length > 0; length -= 2)
    sum += *ptr++;
  sum = (sum >> 16) + (sum & 0xffff);
  return (u_int16_t)(~(sum + (sum >> 16)));
}

struct icmp create_header(int id, int seq) {
  struct icmp header;

  header.icmp_type = ICMP_ECHO;
  header.icmp_code = 0;
  header.icmp_hun.ih_idseq.icd_id = id;
  header.icmp_hun.ih_idseq.icd_seq = seq;
  header.icmp_cksum = 0;
  header.icmp_cksum =
      compute_icmp_checksum((u_int16_t *)&header, sizeof(header));

  return header;
}
ssize_t send_packet(const std::string ip_addr, const int &sockfd,
                    const struct icmp &header, const int ttl) {

  struct sockaddr_in recipient;
  bzero(&recipient, sizeof(recipient));
  recipient.sin_family = AF_INET;
  inet_pton(AF_INET, ip_addr.c_str(), &recipient.sin_addr);

  setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(int));

  ssize_t bytes_sent = sendto(sockfd, &header, sizeof(header), 0,
                              (struct sockaddr *)&recipient, sizeof(recipient));
  return bytes_sent;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Error: Invalid parameters!\n");
    printf("Usage: %s <dst_addr>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  std::string dest_addr = argv[1];

  int sockfd; // create a raw socket with ICMP protocol
  if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
    printf("socket() error\n");
    exit(EXIT_FAILURE);
  }

  bool destination_reached = false;

  for (int i = 1; i <= 30; i++) {
    int ttl = i;

    // send packets
    for (int j = 0; j < 3; j++) {
      int id = cnt, seq = ttl;
      struct icmp header = create_header(id, seq);
      ssize_t bytes_send = send_packet(dest_addr, sockfd, header, ttl);
      if (bytes_send < 0) {
        std::cout << "sendto() error \n";
      }
      packets[id].id = id;
      packets[id].ttl = ttl;
      packets[id].time_start = std::chrono::system_clock::now();
      packets[id].reply_received = 0;
      cnt++;
    }

    // wait for replies
    fd_set descriptors;
    FD_ZERO(&descriptors);
    FD_SET(sockfd, &descriptors);

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    int responses = 0;

    while (responses < 3 && (tv.tv_sec > 0 || tv.tv_usec > 0)) {
      int ready = select(sockfd + 1, &descriptors, NULL, NULL, &tv);

      if (ready < 0) {
        printf("select() error\n");
      } else if (ready == 0) {
        break;
      }

      struct sockaddr_in sender;
      socklen_t sender_len = sizeof(sender);
      u_int8_t buffer[IP_MAXPACKET];

      // receive packets
      for (;;) {
        ssize_t packet_len =
            recvfrom(sockfd, buffer, IP_MAXPACKET, MSG_DONTWAIT,
                     (struct sockaddr *)&sender, &sender_len);

        if (packet_len <= 0) {
          break;
        }

        char sender_ip_str[20];
        inet_ntop(AF_INET, &(sender.sin_addr), sender_ip_str,
                  sizeof(sender_ip_str));

        struct ip *ip_header = (struct ip *)buffer;
        ssize_t ip_header_len = 4 * ip_header->ip_hl;
        u_int8_t *icmp_packet = buffer + 4 * ip_header->ip_hl;
        struct icmp *icmp_header = (struct icmp *)icmp_packet;
        ssize_t icmp_header_len = 8;

        // time exceeded
        if (icmp_header->icmp_type == ICMP_TIMXCEED) {
          ssize_t inner_packet = ip_header_len + icmp_header_len;
          ip_header = (struct ip *)(buffer + inner_packet);
          ip_header_len = 4 * ip_header->ip_hl;
          icmp_packet = buffer + inner_packet + ip_header_len;
          icmp_header = (struct icmp *)icmp_packet;
        }

        u_int16_t id = icmp_header->icmp_hun.ih_idseq.icd_id;
        u_int16_t packet_ttl = icmp_header->icmp_hun.ih_idseq.icd_seq;

        if (packet_ttl == ttl) {

          if (sender_ip_str == dest_addr) {
            destination_reached = true;
          }

          packets[id].time_end = std::chrono::system_clock::now();
          packets[id].time_duration =
              packets[id].time_end - packets[id].time_start;
          packets[id].reply_ip_addr = sender_ip_str;
          packets[id].reply_received = 1;
          responses++;
        }
      }
    }
    print_reply_info(&packets[cnt - 3], &packets[cnt - 2], &packets[cnt - 1]);
    //
    if (destination_reached) {
      break;
    }
  }
}