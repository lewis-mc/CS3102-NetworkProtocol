/*
  THIS FILE MUST NOT BE MODIFIED.

  CS3102 Coursework P2 : Simple, Reliable Transport Protocol (SRTP)
    saleem, Jan2024, Feb2023
    sjm55 (checked February 2024)

  Test server 2 : server upload.
*/

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include "srtp.h"
#include "srtp-common.h"
#include "srtp-pcb.h"


int
main(int argc, char **argv)
{
  uid_t port = getuid(); // same as test-client-2.c
  int start_sd, sd;
  uint8_t *data;
  int b;
  int32_t v1, *v1_p, v2, *v2_p;
  char remote[22];
  int n = 100000; // same as test-client-1.c
  int rx_pkts = 0, rx_fails = 0;

  if (argc != 1) {
    printf("usage: test-server-2\n");
    exit(0);
  }
  printf("test-server-2\n");

  if (port < 1024)
    port += 1024; // hack, avoid reserved ports, should not happen in labs

  srtp_initialise();

  start_sd = srtp_start(port);
  if (start_sd < 0) {
    printf("start() failed\n");
    exit(0);
  }
  printf("start() OK\n");

  sd = srtp_accept(start_sd);
  if (sd < 0) {
    printf("accept() failed\n");
    exit(0);
  }
  SrtpPcb_remote(remote);
  printf("accept(): %s\n", remote);

  data = malloc(SRTP_MAX_DATA_SIZE); /* SRTP_MAX_DATA_SIZE = 1280 */
  v1_p = (int32_t *) data; // first word
  v2_p = (int32_t *) &data[SRTP_MAX_DATA_SIZE - sizeof(int32_t)]; // last word

  /* receive n packets */
  for (int i = 0; i < n; ++i) {

    memset(data, 0, SRTP_MAX_DATA_SIZE); // clear
    b = srtp_rx(sd, data, SRTP_MAX_DATA_SIZE);

    if (b < 0) {
      printf("srtp_rx() failed (n=%d)\n", n);
      ++rx_fails;
      continue;
    }
    ++rx_pkts;
  
    /* Is size as expected? */
    if (b == SRTP_MAX_DATA_SIZE)
      { v1 = ntohl(*(v1_p)); v2 = ntohl(*(v2_p)); }
    else { v1 = v2 = -1; }

    /* Is content as expected? */
#define CHECK_DATA(v_, c_) (v_ == c_ ? "(good)" : "(bad)")
    int c = i + 1;
    printf("srtp_rx(): %d/%d pkts, %d bytes %s, v1=%d %s, v2=%d %s.\n",
            c, n,
            b, CHECK_DATA(b, SRTP_MAX_DATA_SIZE),
            v1, CHECK_DATA(v1, c),
            v2, CHECK_DATA(v2, c));

  } // for (int i = 0; i < n; ++i)
 
  /*
    finish
  */
  printf(" %d/%d pkts from %s, %d fails.\n", rx_pkts, n, remote, rx_fails);
  srtp_close(sd);
  SrtpPcb_report();
  free(data);

  return 0;
}
