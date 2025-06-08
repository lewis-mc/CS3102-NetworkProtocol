#ifndef __srtp_fsm_h__
#define __srtp_fsm_h__

/*
  CS3102 Coursework P2 : Simple, Reliable Transport Protocol (SRTP)

  saleem, Jan2024, Feb2023
  sjm55 (checked February 2024)
*/

/* CS3102: you can use the states defined below, or define your own */

typedef enum SRTP_state_e {
  SRTP_state_error = -1, // -1 problem with state
  SRTP_state_closed,     //  0 connection closed
  SRTP_state_listening,  //  1 @server : listening for incoming connections
  SRTP_state_opening,    //  2 @client : sent request to start connection
  SRTP_state_connected,  //  3 connected : connection established
  SRTP_state_closing_i,  //  4 closing initiated (@server or @client)
  SRTP_state_closing_r   //  5 closing responder (@server or @client)
} SRTP_state_t;

/* CS3102: add anything else here if needed */

#endif /* __srtp_fsm_h__ */
