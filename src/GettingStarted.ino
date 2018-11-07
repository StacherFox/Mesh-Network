#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

#define MTU 32

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(9,10);

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

// Set up role.
typedef enum { role_ping_out = 1, role_pong_back } role_e;

// The debug-friendly names of those roles
const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};

// The role of the current running sketch
role_e role = role_ping_out;

// Ip Configuration
uint8_t destIp[] = {172, 16, 5, 5};
uint8_t srcIp[] = {172, 16, 5, 8};

/*
 * IP Header
 */
 #define IP_VER_HLEN     0x45
 #define IP_HEADER_LEN   sizeof(NetIpHdr)
 #define IP_DATA_LEN     (MTU - IP_HEADER_LEN)

typedef struct
{
    uint8_t   ver_hlen;   /* Header version and length (dwords). */
    uint8_t   service;    /* Service type. */
    uint16_t  length;     /* Length of datagram (bytes). */
    uint16_t  ident;      /* Unique packet identification. */
    uint16_t  fragment;   /* Flags; Fragment offset. */
    uint8_t   timetolive; /* Packet time to live (in network). */
    uint8_t   protocol;   /* Upper level protocol (UDP, TCP). */
    uint16_t  checksum;   /* IP header checksum. */
    uint32_t  src_addr;   /* Source IP address. */
    uint32_t  dest_addr;  /* Destination IP address. */
} NetIpHdr;

/*
 * IP Packet
 */
typedef struct
{
    NetIpHdr ipHdr;
    uint8_t  data[IP_DATA_LEN];
} NetIpPkt;

// Global NetIpPkt
NetIpPkt ipPacket;


void setup(void)
{
  // Print preamble
  Serial.begin(57600);
  printf_begin();
  printf("\n\r\rRF24/examples/GettingStarted/\n\r\r");
  printf("ROLE: %s\n\r\r",role_friendly_name[role]);
  printf("*** PRESS 'T' to begin transmitting to the other node\n\r\r");

  // Setup and configure rf radio
  radio.begin();

  // optionally, increase the delay between retries & # of retries
  radio.setRetries(15,15);

  //if ( role == role_ping_out )
  {
    //radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
  }
  //else
  {
    //radio.openWritingPipe(pipes[1]);
    //radio.openReadingPipe(1,pipes[0]);
  }

  // Start listening
  radio.startListening();

  // Dump the configuration of the rf unit for debugging
  radio.printDetails();
}


bool NetIpSnd(NetIpPkt * pIpPkt, uint16_t len) {
  uint16_t ident;
  ident = 1;

  // First, stop listening so we can talk.
  radio.stopListening();

  pIpPkt->ipHdr.ver_hlen = IP_VER_HLEN;
  pIpPkt->ipHdr.service = 0x00;
  pIpPkt->ipHdr.length = len;
  pIpPkt->ipHdr.ident = ident;
  pIpPkt->ipHdr.fragment = 0x00;
  pIpPkt->ipHdr.timetolive = 0x10;

  pIpPkt->ipHdr.checksum = checksum(&(pIpPkt->ipHdr), sizeof(pIpPkt->ipHdr));

  bool ok = radio.write(pIpPkt, sizeof(NetIpPkt));

  return ok;
}


bool NetIpRcv(NetIpPkt * pIpPkt){
  uint16_t checksum;

  radio.startListening();

  // Wait here until we get a response, or timeout (250ms)
  unsigned long started_waiting_at = millis();
  bool timeout = false;
  while ( ! radio.available() && ! timeout )
    if (millis() - started_waiting_at > 200 )
      timeout = true;

  // Describe the results
  if ( timeout )
  {
    return false;
    printf("NetIpRcv: timeout\n\r");
  }
  else
  {
    // Grab the response, compare, and send to debugging spew
    bool ok = radio.read(pIpPkt, sizeof(NetIpPkt) );

    if(!ok){
      printf("NetIpRcv: not ok\n\r");
      return false;
    }
  }


  /*
   * Check IP header version and length.
   */
  if (pIpPkt->ipHdr.ver_hlen != IP_VER_HLEN)
  {
    printf("NetIpRcv: Ip version error\n\r");
    /*
     * Unsupported header version or length.
     */
      return false;
  }

  /*
   * Move the IP header checksum out of the header.
   */
  checksum = pIpPkt->ipHdr.checksum;
  pIpPkt->ipHdr.checksum = 0;

  /*
   * Compute checksum and compare with received value.
   */
  if (checksum != ::checksum(&pIpPkt->ipHdr, sizeof(pIpPkt->ipHdr)))
  {
    printf("NetIpRcv: bad checksum\n\r");
    return false; //Bad checksum
  }

  printf("NetIpRcv: Packet Received:\n\r");
  printf("NetIpRcv: Source Ip: ");
  printIp(&pIpPkt->ipHdr.src_addr);
  printf("NetIpRcv: Destination Ip: ");
  printIp(&pIpPkt->ipHdr.dest_addr);

  if(pIpPkt->ipHdr.dest_addr == *((uint32_t*)srcIp)){
    printf("NetIpRcv: IP Pass\n\r");
    return true;
  } else {
    printf("NetIpRcv: IP Wrong\n\r");
    return false;
  }



}

void printIp(uint32_t* ip){
  printf("%u.%u.%u.%u\n\r", ((uint8_t*)ip)[0], ((uint8_t*)ip)[1], ((uint8_t*)ip)[2], ((uint8_t*)ip)[3] );
}

void fillIp(uint32_t* destAddr, uint8_t* ipAddr){
  *destAddr = *((uint32_t*)ipAddr);
}


void loop(void)
{

  // Ping out role.
  if (role == role_ping_out)
  {
    fillIp(&ipPacket.ipHdr.dest_addr, destIp);
    fillIp(&ipPacket.ipHdr.src_addr, srcIp);

    char stringToSend[] = "batata";
    strcpy(ipPacket.data, stringToSend);

    printf("Sending Message: %s\n\r", stringToSend);

    bool ok = NetIpSnd(&ipPacket, sizeof(stringToSend));

    if (ok)
      printf("Send ok...\n\r");
    else
      printf("Send failed.\n\r");

    ok = NetIpRcv(&ipPacket);

    if(ok)
      printf("Message Received: %s\n\r", ipPacket.data);
    else
      printf("Response Not Received\n\r");

    printf("\n\r");
    // Try again 1s later
    delay(1000);
  }

  // Pong back role.  Receive each packet, dump it out, and send it back
  if ( role == role_pong_back )
  {
    while(!(NetIpRcv(&ipPacket)));
    printf("Message Received: %s\n\r", ipPacket.data);

    // Delay just a little bit to let the other unit
  	// make the transition to receiver
  	delay(20);

    fillIp(&ipPacket.ipHdr.dest_addr, destIp);
    fillIp(&ipPacket.ipHdr.src_addr, srcIp);

    char stringToSend[] = "resp";
    strcpy(ipPacket.data, stringToSend);

    printf("Sending Message: %s\n\r", stringToSend);

    bool ok = NetIpSnd(&ipPacket, sizeof(stringToSend));

    if (ok)
      printf("Send ok...\n\r");
    else
      printf("Send failed.\n\r\r");

    printf("\n\r");
  }

  // Change roles
  if ( Serial.available() )
  {
    char c = toupper(Serial.read());
    if ( c == 'T' && role == role_pong_back )
    {
      printf("*** CHANGING TO TRANSMIT ROLE -- PRESS 'R' TO SWITCH BACK\n\r\r");

      // Become the primary transmitter (ping out)
      role = role_ping_out;
      radio.openWritingPipe(pipes[0]);
      radio.openReadingPipe(1,pipes[1]);
    }
    else if ( c == 'R' && role == role_ping_out )
    {
      printf("*** CHANGING TO RECEIVE ROLE -- PRESS 'T' TO SWITCH BACK\n\r\r");

      // Become the primary receiver (pong back)
      role = role_pong_back;
      radio.openWritingPipe(pipes[1]);
      radio.openReadingPipe(1,pipes[0]);
    }
  }
}

uint16_t checksum(void *addr, int count)
{
    /* Compute Internet Checksum for "count" bytes
     *         beginning at location "addr".
     * Taken from https://tools.ietf.org/html/rfc1071
     */

    register uint32_t sum = 0;
    uint16_t * ptr = addr;

    while( count > 1 )  {
        /*  This is the inner loop */
        sum += * ptr++;
        count -= 2;
    }

    /*  Add left-over byte, if any */
    if( count > 0 )
        sum += * (uint8_t *) ptr;

    /*  Fold 32-bit sum to 16 bits */
    while (sum>>16)
        sum = (sum & 0xffff) + (sum >> 16);

    return ~sum;
}
