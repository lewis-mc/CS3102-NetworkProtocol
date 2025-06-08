#ifndef __srtp_packet_h__
#define __srtp_packet_h__

/*
  CS3102 Coursework P2 : Simple, Reliable Transport Protocol (SRTP)
  saleem, Jan2024, Feb2023
  sjm55 (checked February 2024)

  These are suggested definitions only.
  Please modify as required.
*/

#include "srtp-common.h"

/* packet type values : bit field, but can be used as required */

#define SRTP_TYPE_req       ((uint8_t) 0x01)
#define SRTP_TYPE_ack       ((uint8_t) 0x02)

#define SRTP_TYPE_open      ((uint8_t) 0x10)
#define SRTP_TYPE_open_req  (SRTP_TYPE_open  | SRTP_TYPE_req)
#define SRTP_TYPE_open_ack  (SRTP_TYPE_open  | SRTP_TYPE_ack)

#define SRTP_TYPE_close     ((uint8_t) 0x20)
#define SRTP_TYPE_close_req (SRTP_TYPE_close | SRTP_TYPE_req)
#define SRTP_TYPE_close_ack (SRTP_TYPE_close | SRTP_TYPE_ack)

#define SRTP_TYPE_data      ((uint8_t) 0x40)
#define SRTP_TYPE_data_req  (SRTP_TYPE_data  | SRTP_TYPE_req)
#define SRTP_TYPE_data_ack  (SRTP_TYPE_data  | SRTP_TYPE_ack)


typedef struct Srtp_Header_s {

  /* This header should not have a memory image greater than 100 bytes */

  /* CS3102 : define your header here */

  uint64_t timestamp;
  uint32_t seq_num;
  uint16_t type;

} Srtp_Header_t;

#define SRTP_MAX_PAYLOAD_SIZE SRTP_MAX_DATA_SIZE

typedef struct Srtp_Packet_s {

  /* CS3102 : define your packet here */

  Srtp_Header_t header;
  uint8_t payload[SRTP_MAX_PAYLOAD_SIZE];

} Srtp_Packet_t;


/* CS3012 : put in here whatever else you need */

#endif /* __srtp_packet_h__ */
