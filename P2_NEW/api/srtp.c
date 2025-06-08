/*
  CS3201 Coursework P2 : Simpler Reliable Transport Protocol (SRTP).
  saleem, Jan2024, Feb2023.
  sjm55 (checked February 2024)

  API for SRTP.

  Underneath, it MUST use UDP. 
*/

#include <stdio.h> /* this should have 'void perror(const char *s);' */
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#include "srtp.h"
#include "srtp-packet.h"
#include "srtp-common.h"
#include "srtp-fsm.h"
#include "srtp-pcb.h"

#include "byteorder64.h"
#include "d_print.h"

extern Srtp_Pcb_t G_pcb; /* in srtp-pcb.c */

/* CS3102: Add anything you need below here - modify this file as you need to */

#define OPEN_REQ_TYPE   ((uint16_t)0)
#define OPEN_ACK_TYPE   ((uint16_t)1)
#define DATA_REQ_TYPE   ((uint16_t)2)
#define DATA_ACK_TYPE   ((uint16_t)3)
#define CLOSE_REQ_TYPE  ((uint16_t)4)
#define CLOSE_ACK_TYPE  ((uint16_t)5)


Srtp_Packet_t last_packet_recv;
Srtp_Packet_t last_packet_sent;
int sent_data_ack = 0;
int recv_first_data_ack = 1;
int send_data_ack = 0;
int RTO = 500000000;
int min_rto = 150000000;
int adaptive_rto = 1; // 1 - on, 0 - off
double est_rtt = 0;
double dev_rtt = 0;
int dropped_ack = 0;
int recv_first_close_req = 0;

int drop_rate_req = 10;
int drop_rate_ack = 10;




int recv_packet(int sd, struct sockaddr_in *remote, socklen_t *socket_len, Srtp_Packet_t *packet, uint16_t expected_type, void* data, uint16_t data_size);
void create_packet(uint16_t type, uint32_t seq_num, uint8_t *payload,Srtp_Packet_t *packet, uint16_t data_size);
int send_packet(int sd, struct sockaddr_in *remote, socklen_t socket_len, Srtp_Packet_t *packet);
void initialiseRandomNumberGenerator();
int shouldDropPacket(int dropRate);
int get_rtt(int time_now, int ack_time);
void update_rto(double rtt_sample);

/*
  Must be called before any other srtp_zzz() API calls.

  For use by client and server process.
*/
void
srtp_initialise()
{
  /*
    CS3012 : modify as required
  */

  reset_SrtpPcb();
}



/*
  port : local port number to be used for socket
  return : error - srtp-common.h
           success - valid socket descriptor
  
  For use by server process.
*/
int
srtp_start(uint16_t port)
{

  /*
    CS3012 : modify as required
  */

  int sd;
  struct sockaddr_in local;

  // Opens the UDP socket
  if ((sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("srtp_start(): socket");
    return SRTP_ERROR;
  }

  // Set up the local address
  memset((void *) &local, 0, sizeof(local));
  local.sin_family = AF_INET;
  local.sin_port = htons(port);
  local.sin_addr.s_addr = htonl(INADDR_ANY);

  // Bind the socket to the local address
  if (bind(sd, (struct sockaddr *) &local, sizeof(local)) < 0) {
    perror("srtp_start(): bind");
    return SRTP_ERROR;
  }

  // Set the PCB values
  G_pcb.sd = sd;
  G_pcb.port = port;
  G_pcb.state = SRTP_state_listening;
  G_pcb.local = local;

  return sd;

}


/*
  sd : socket descriptor as previously provided by srtp_start()
  return : error - srtp-common.h
           success - sd, to indicate sd now also is "connected"
  
  For use by server process.
*/
int
srtp_accept(int sd)
{
  /*
    CS3012 : modify as required
  */

  struct sockaddr_in remote;
  socklen_t socket_len = sizeof(struct sockaddr);
  Srtp_Packet_t open_req;
  
  int b = recv_packet(sd, &remote, &socket_len, &open_req, OPEN_REQ_TYPE, NULL, 0); // waits to receive open_req
  G_pcb.start_time = srtp_timestamp();
  if (b < 0) {
    perror("srtp_accept(): recv_packet ");
    return SRTP_ERROR;
  }

  G_pcb.open_req_rx++;

  Srtp_Packet_t open_ack;
  create_packet(OPEN_ACK_TYPE, 0, NULL, &open_ack, 0);
  b = send_packet(sd, &remote, sizeof(struct sockaddr), &open_ack); // sends open_ack

  if (b != SRTP_SUCCESS) {
    perror("srtp_accept(): send_packet");
    return SRTP_ERROR;    
  }

  G_pcb.open_ack_tx++;
  G_pcb.sd = sd;
  G_pcb.remote = remote;
  G_pcb.state = SRTP_state_connected;  

  return sd;
}


/*
  port : local and remote port number to be used for socket
  return : error - SRTP_ERROR
           success - valid socket descriptor
  
  For use by client process.
*/
int
srtp_open(const char *fqdn, uint16_t port)
{
  /*
    CS3012 : modify as required
  */

  // Sets up UDP Socket

  int sd;
  struct sockaddr_in remote;
  socklen_t socket_len = sizeof(struct sockaddr);

  if ((sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("srtp_open(): socket");
    return SRTP_ERROR;
  } 

  memset((void *) &remote, 0, sizeof(remote));
  remote.sin_family = AF_INET;
  remote.sin_port = htons(port);
  struct in_addr addr;
  addr.s_addr = (in_addr_t) 0;
  struct hostent *host = gethostbyname(fqdn);
  if (host == (struct hostent *) 0) {
    perror("srtp_open(): gethostbyname()");
  } else {
    memcpy((void *) &addr.s_addr,
    (void *) *host->h_addr_list, sizeof(addr.s_addr));
  }

  remote.sin_addr = addr;

  Srtp_Packet_t open_req;
  int b;

  create_packet(OPEN_REQ_TYPE, 0, NULL, &open_req, 0);
  b = send_packet(sd, &remote, sizeof(struct sockaddr), &open_req); // sends open_req
   G_pcb.start_time = srtp_timestamp();

  if (b != SRTP_SUCCESS) {
    perror("srtp_open(): send_packet ");
    return SRTP_ERROR;
  }

  G_pcb.open_req_tx++;
  G_pcb.state = SRTP_state_opening;

  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(sd, &readfds);

  struct timespec timeout;
  timeout.tv_sec = 0;
  timeout.tv_nsec = RTO;
  int open_ack_recv = 0;


  while (open_ack_recv == 0) { // while open_ack not received

    if (pselect(sd+1, &readfds, NULL, NULL, &timeout, NULL) > 0) { // starts a timeout
 
      if (FD_ISSET(sd, &readfds)) {

        Srtp_Packet_t open_ack;
        int b;
        b = recv_packet(sd, &remote, &socket_len, &open_ack, OPEN_ACK_TYPE, NULL, 0); // received open_ack

        if (b < 0) {
          perror("srtp_open(): recv_packet ");
          return SRTP_ERROR;
        }

        G_pcb.open_ack_rx++;
        G_pcb.sd = sd;
        G_pcb.port = port;
        G_pcb.remote = remote;
        G_pcb.state = SRTP_state_connected;

        break;

      }

    } else { // timed out, sends open_req again and waits to receive open_ack
      Srtp_Packet_t open_req;
      create_packet(OPEN_REQ_TYPE, 0, NULL, &open_req, 0);

      b = send_packet(sd, &remote, sizeof(struct sockaddr), &open_req);

      if (b != SRTP_SUCCESS) {
        perror("srtp_open(): sendto");
        return SRTP_ERROR;
      }
    }
    
    if (G_pcb.state == SRTP_state_connected) {
      open_ack_recv = 1;
    }
  
  }

  return sd;

}




/*
  port : local and remote port number to be used for socket
  return : error - SRTP_ERROR
           success - SRTP_SUCCESS
  
  For use by client process.
*/
int
srtp_close(int sd)
{
  /*
    CS3012 : modify as required
  */

  int b;
  struct sockaddr_in remote;
  remote = G_pcb.remote;
  socklen_t socket_len = sizeof(struct sockaddr);

  int close_req_recv = 0;
  int close_ack_recv = 0;

  while (close_req_recv == 0) {

    Srtp_Packet_t close_req;
    create_packet(CLOSE_REQ_TYPE, 0, NULL, &close_req, 0);
    b = send_packet(sd, &remote, sizeof(struct sockaddr), &close_req);

    G_pcb.close_req_tx++;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sd, &readfds);

    struct timespec timeout;
    long new_nsec = RTO * 3;
    timeout.tv_sec = new_nsec / 1000000000;
    timeout.tv_nsec = new_nsec % 1000000000;

    if (timeout.tv_nsec >= 1000000000) {
      timeout.tv_sec += timeout.tv_nsec / 1000000000; // Handle additional overflow
      timeout.tv_nsec = timeout.tv_nsec % 1000000000;
    }

    if (recv_first_close_req == 1) {
      break;
    }

    if (pselect(sd+1, &readfds, NULL, NULL, &timeout, NULL) > 0) {
      Srtp_Packet_t close_req_packet_recv;
      b = recv_packet(sd, &remote, &socket_len, &close_req_packet_recv, CLOSE_REQ_TYPE, NULL, 0);
      if (b < 0) {
        perror("srtp_close(): recv_packet ");
        return SRTP_ERROR;
      }
      close_req_recv = 1;
      G_pcb.close_req_rx++;
    } else {
      close_req_recv = 0;
    }
  }

  while (close_ack_recv == 0) {
    Srtp_Packet_t close_ack;
    create_packet(CLOSE_ACK_TYPE, 0, NULL, &close_ack, 0);
    b = send_packet(sd, &remote, sizeof(struct sockaddr), &close_ack);
    G_pcb.close_ack_tx++;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sd, &readfds);

    struct timespec timeout;
    long new_nsec = RTO * 3;
    timeout.tv_sec = new_nsec / 1000000000;
    timeout.tv_nsec = new_nsec % 1000000000;

    if (timeout.tv_nsec >= 1000000000) {
      timeout.tv_sec += timeout.tv_nsec / 1000000000; // Handle additional overflow
      timeout.tv_nsec = timeout.tv_nsec % 1000000000;
    }

    if (pselect(sd+1, &readfds, NULL, NULL, &timeout, NULL) > 0) {
      Srtp_Packet_t close_ack_packet_recv;
      b = recv_packet(sd, &remote, &socket_len, &close_ack_packet_recv, CLOSE_ACK_TYPE, NULL, 0);
      if (b < 0) {
        perror("srtp_close(): recv_packet ");
        return SRTP_ERROR;
      }
      close_ack_recv = 1;
      G_pcb.close_ack_rx++;
    } else {
      close_ack_recv = 0;
    }
  }

  G_pcb.state = SRTP_state_closed;
 

  return SRTP_SUCCESS;
}


/*
  sd : socket descriptor
  data : buffer with bytestream to transmit
  data_size : number of bytes to transmit
  return : error - SRTP_ERROR
           success - number of bytes transmitted
*/
int
srtp_tx(int sd, void *data, uint16_t data_size)
{
  /*
    CS3012 : modify as required
  */

  int b;
  struct sockaddr_in remote;
  socklen_t socket_len = sizeof(struct sockaddr);
  remote = G_pcb.remote;
  int received_data_ack = 0;
  int send_data_req = 1;

  while (received_data_ack != 1) { // not received data_ack

    if (send_data_req == 1) {
      Srtp_Packet_t data_req;
      create_packet(DATA_REQ_TYPE, G_pcb.seq_tx, data, &data_req, data_size);
      b = send_packet(sd, &remote, sizeof(struct sockaddr), &data_req); // sends data_req
      if (b < 0) {
        perror("srtp_tx(): send_packet ");
        return SRTP_ERROR;
      }
      send_data_req = 0; 
    }

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sd, &readfds);

    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = (long) RTO;

    if (pselect(sd+1, &readfds, NULL, NULL, &timeout, NULL) > 0) {
      if (FD_ISSET(sd, &readfds)) {
        Srtp_Packet_t data_ack;
        if (shouldDropPacket(drop_rate_ack)) { // data_ack dropped
          //printf("DROPPED ACK -----> ");
          dropped_ack = 1;
          b = recv_packet(sd, &remote, &socket_len, &data_ack, DATA_ACK_TYPE, NULL, 0);
          dropped_ack = 0;
          if (b < 0) {
            perror("srtp_tx() recv_packet ");
            return SRTP_ERROR;
          }
          G_pcb.data_req_re_tx++;
          G_pcb.data_req_bytes_re_tx += data_size;
        } else { // data_ack received
          b = recv_packet(sd, &remote, &socket_len, &data_ack, DATA_ACK_TYPE, NULL, 0);
          if (b < 0) {
            perror("srtp_tx() recv_packet ");
            return SRTP_ERROR;
          }
          G_pcb.data_ack_rx++;
          G_pcb.seq_tx++;
          received_data_ack = 1;
        }
      }
    } else { // timeout
      send_data_req = 1;
    }
  }

  G_pcb.data_req_bytes_tx += data_size;
  G_pcb.data_req_tx++;

  return data_size;
}

/*
  tx receives a data_req when expecting a data_ack
*/


/*
  sd : socket descriptor
  data : buffer to store bytestream received
  data_size : size of buffer
  return : error - SRTP_ERROR
           success - number of bytes received
*/
int
srtp_rx(int sd, void *data, uint16_t data_size)
{
  /*
    CS3012 : modify as required
  */

  int b;
  struct sockaddr_in remote;
  remote = G_pcb.remote;
  socklen_t socket_len = sizeof(struct sockaddr);
  int sent_data_ack = 0;

  while (sent_data_ack == 0) { // data_ack not sent successfully

    Srtp_Packet_t data_req_first;

    if (recv_first_data_ack == 1) { // receives first data_req
      
      b = recv_packet(sd, &remote, &socket_len, &data_req_first, DATA_REQ_TYPE, data, data_size); // receives data_req
      if (b < 0) {
        perror("srtp_rx() recv_packet ");
        return SRTP_ERROR;
      }

      G_pcb.data_req_rx++;
      G_pcb.data_req_bytes_rx += data_size;
      last_packet_recv = data_req_first;
      send_data_ack = 1;
    }

    if (send_data_ack == 1) { // sends data_ack
      Srtp_Packet_t data_ack;
      create_packet(DATA_ACK_TYPE, G_pcb.seq_rx, NULL, &data_ack, 0);
      b = send_packet(sd, &remote, sizeof(struct sockaddr), &data_ack);
      if (b < 0) {
        perror("srtp_tx(): send_packet ");
        return SRTP_ERROR;
      }
      last_packet_sent = data_ack;
    } else { // sends the last data_ack
      b = send_packet(sd, &remote, sizeof(struct sockaddr), &last_packet_sent);
      if (b < 0) {
        perror("srtp_tx(): send_packet ");
        return SRTP_ERROR;
        send_data_ack = 1;
        G_pcb.seq_rx--;
      }
    }

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sd, &readfds);

    struct timespec timeout;
    long new_nsec = RTO * 3;
    timeout.tv_sec = new_nsec / 1000000000;
    timeout.tv_nsec = new_nsec % 1000000000;
    if (timeout.tv_nsec >= 1000000000) {
      timeout.tv_sec += timeout.tv_nsec / 1000000000; // Handle additional overflow
      timeout.tv_nsec = timeout.tv_nsec % 1000000000;
    }

    // waits for next data_req
    if (pselect(sd+1, &readfds, NULL, NULL, &timeout, NULL) > 0) {

      if (FD_ISSET(sd, &readfds)) {

        if (shouldDropPacket(drop_rate_req)) {
          //printf("DROPPED REQ ------>");
          Srtp_Packet_t data_req;
          b = recv_packet(sd, &remote, &socket_len, &data_req, DATA_REQ_TYPE, data, data_size);
        } else {
          Srtp_Packet_t data_req;
          b = recv_packet(sd, &remote, &socket_len, &data_req, DATA_REQ_TYPE, data, data_size); // receives 'next' data_req
          if (b < 0) {
            perror("srtp_rx(): recv_packet ");
            return SRTP_ERROR;
          }
          if (b == CLOSE_REQ_TYPE) {
            recv_first_close_req = 1;
            G_pcb.close_req_rx++;
            return data_size;
          }
          if (recv_first_data_ack == 1) {
            memcpy(data, data_req_first.payload, data_size);
            recv_first_data_ack = 0;
          }

          if (last_packet_recv.header.seq_num == data_req.header.seq_num) { // needs to send the data_ack again
            send_data_ack = 0;
            recv_first_data_ack = 0;
            G_pcb.data_req_dup_rx++;
            G_pcb.data_req_bytes_dup_rx += data_size;

          } else { // continue and read next data_ack
            memcpy(data, last_packet_recv.payload, data_size);
            last_packet_recv = data_req;
            G_pcb.seq_rx++;
            G_pcb.data_req_rx++;
            G_pcb.data_req_bytes_rx += data_size;
            sent_data_ack = 1;
          }
        }
      }

    } else {
      G_pcb.seq_rx++;
      send_data_ack = 1;
      sent_data_ack = 1;
      recv_first_data_ack = 1;
    }
  }


  return data_size;
}


int recv_packet(int sd, struct sockaddr_in *remote, socklen_t *socket_len, Srtp_Packet_t *packet, uint16_t expected_type, void* data, uint16_t data_size) {
  
  int b;
  uint8_t *buffer[sizeof(Srtp_Header_t) + data_size];

  if ((b = recvfrom(sd, buffer, (sizeof(Srtp_Header_t) + data_size), 0, (struct sockaddr *) remote,  socket_len)) < 0) {
    perror("recv_packet(): recvfrom ");
    return SRTP_ERROR;
  }

  memcpy(packet, buffer, sizeof(Srtp_Header_t) + data_size);

  packet->header.timestamp = ntoh64(packet->header.timestamp);
  packet->header.seq_num = ntohl(packet->header.seq_num);
  packet->header.type = ntohs(packet->header.type); 

  if (data_size != 0 && packet->header.type != CLOSE_REQ_TYPE) {
    memcpy(data, packet->payload, data_size);
  } 

  //printf("REVC PACKET: %d TIME: %lu SEQ_NUM: %d\n", packet->header.type, packet->header.timestamp, packet->header.seq_num);

  if (adaptive_rto == 1 && packet->header.type == DATA_ACK_TYPE && dropped_ack != 1) {
    struct timeval now;
    gettimeofday(&now, NULL);
    int time_now = (int) now.tv_sec * 1000000 + now.tv_usec;

    update_rto(get_rtt(time_now ,(int) packet->header.timestamp));
    if (RTO < min_rto) {
      RTO = min_rto;
    }
  }

  if (packet->header.type != expected_type) {
    return packet->header.type;
  }

  return b; 

}

void create_packet(uint16_t type, uint32_t seq_num, uint8_t *payload, Srtp_Packet_t *packet, uint16_t data_size) {
  
  packet->header.type = htons(type);
  packet->header.seq_num = htonl(seq_num);
  struct timeval tv;
  gettimeofday(&tv, NULL);
  packet->header.timestamp = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
  packet->header.timestamp = hton64(packet->header.timestamp);
  if (data_size != 0) {
    memcpy(packet->payload, payload, data_size); 
  }
  
}

int send_packet(int sd, struct sockaddr_in *remote, socklen_t socket_len, Srtp_Packet_t *packet) {

  int b;

  if ((b = sendto(sd, packet, sizeof(Srtp_Packet_t), 0, (struct sockaddr *) remote, socket_len)) < 0) {
    perror("send_packet(): sendto");
    return SRTP_ERROR;
  }

  //printf("SENT PACKET: %d TIME: %lu SEQ_NUM: %d\n", ntohs(packet->header.type), ntoh64(packet->header.timestamp), ntohl(packet->header.seq_num));

  return SRTP_SUCCESS;

}

void initialiseRandomNumberGenerator() {
  srand(time(NULL));
}

int shouldDropPacket(int dropRate) {
  int randomNumber = rand() % 100;
  return randomNumber < dropRate;
}

int get_rtt(int time_now, int ack_time) {
  return time_now - ack_time;
}

void update_rto(double rtt_sample) {

  if (est_rtt == 0) {
    est_rtt = rtt_sample;
  } else {
    est_rtt = (0.875 * est_rtt) + (0.125 * rtt_sample);
    dev_rtt = (0.75 * dev_rtt) + (0.25 * fabs(rtt_sample - est_rtt));
  }

  RTO = (int) (est_rtt + (4 * dev_rtt));
  printf("RTO: %d\n", RTO);

    
}