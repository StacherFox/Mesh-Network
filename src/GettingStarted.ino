#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

#define MTU 32

//#define CLIENT_NUMBER_1

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10
RF24 radio(9,10);

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

// Set up role.
typedef enum { role_ping_out = 1, role_pong_back } role_e;

// The debug-friendly names of those roles
const char* role_friendly_name[] = { "invalid", "Ping out", "Pong back"};

// Ip Configuration
#ifdef CLIENT_NUMBER_1
#define ID_SEED 11000
role_e role = role_ping_out;
uint8_t srcIp[] = {172, 16, 5, 1};
uint8_t destIp[] = {172, 16, 5, 3};
uint16_t  srcPort = 300;
uint16_t  destPort = 500;
#endif

#ifdef CLIENT_NUMBER_2
#define ID_SEED 12000
role_e role = role_pong_back;
uint8_t srcIp[] = {172, 16, 5, 2};
uint8_t destIp[] = {172, 16, 5, 1};
uint16_t  srcPort = 400;
uint16_t  destPort = 300;
#endif

#ifdef CLIENT_NUMBER_3
#define ID_SEED 13000
role_e role = role_pong_back;
uint8_t srcIp[] = {172, 16, 5, 3};
uint8_t destIp[] = {172, 16, 5, 2};
uint16_t  srcPort = 500;
uint16_t  destPort = 400;
#endif

#ifdef CLIENT_NUMBER_4
#define ID_SEED 14000
role_e role = role_pong_back;
uint8_t srcIp[] = {172, 16, 5, 3};
uint8_t destIp[] = {172, 16, 5, 2};
uint16_t  srcPort = 500;
uint16_t  destPort = 400;
#endif

/*
 * IP Header
 */
 #define IP_VER_HLEN     0x45
 #define IP_HEADER_LEN   sizeof(NetIpHdr)
 #define IP_DATA_LEN     (MTU - IP_HEADER_LEN)
 #define UDP_DATA_LEN    (IP_DATA_LEN - 8)

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

/*
* UDP Packet
*/
typedef struct
{
  uint16_t src_port;
  uint16_t dest_port;
  uint16_t length;
  uint16_t checksum;
  uint8_t  data[UDP_DATA_LEN];
} UdpPkt;


uint16_t id = ID_SEED;

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
  radio.setAutoAck(false);
  // optionally, increase the delay between retries & # of retries
  radio.setRetries(15,15);

  if ( role == role_ping_out )
  {
    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
  }
  else
  {
    radio.openWritingPipe(pipes[1]);
    radio.openReadingPipe(1,pipes[0]);
  }

  // Start listening
  radio.startListening();

  // Dump the configuration of the rf unit for debugging
  radio.printDetails();
}

bool NetIpSnd(NetIpPkt pIpPkt) {
  // First, stop listening so we can talk.
  radio.stopListening();

  bool ok = radio.write(&pIpPkt, sizeof(pIpPkt));
  if(ok)
    routingSend(pIpPkt);

  return ok;
}

bool NetIpSnd(void* data, uint16_t len) {
  NetIpPkt pIpPkt;
  uint16_t ident;
  ident = id++;

  // First, stop listening so we can talk.
  radio.stopListening();

  memcpy(pIpPkt.data, data, len);

  fillIp(&pIpPkt.ipHdr.dest_addr, destIp);
  fillIp(&pIpPkt.ipHdr.src_addr, srcIp);

  pIpPkt.ipHdr.ver_hlen = IP_VER_HLEN;
  pIpPkt.ipHdr.service = 0x00;
  pIpPkt.ipHdr.length = len;
  pIpPkt.ipHdr.ident = ident;
  pIpPkt.ipHdr.fragment = 0x00;
  pIpPkt.ipHdr.timetolive = 0x10;
  pIpPkt.ipHdr.checksum = 0;

  pIpPkt.ipHdr.checksum = checksum(&(pIpPkt.ipHdr), sizeof(pIpPkt.ipHdr));

  bool ok = radio.write(&pIpPkt, sizeof(pIpPkt));
  if(ok)
    routingSend(pIpPkt);

  return ok;
}


bool NetIpRcv(void* data, uint16_t* len){
  NetIpPkt pIpPkt;
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
    printf("NetIpRcv: timeout\n\r");
    return false;
  }
  else
  {
    // Grab the response, compare, and send to debugging spew
    bool ok = radio.read(&pIpPkt, sizeof(pIpPkt) );

    if(!ok){
        printf("NetIpRcv: read not ok\n\r");
        return false;
    }
  }

  //Check IP header version and length.
  if (pIpPkt.ipHdr.ver_hlen != IP_VER_HLEN)
  {
    printf("NetIpRcv: Ip version error\n\r");
    //Unsupported header version or length.
    return false;
  }

  //Move the IP header checksum out of the header.
  checksum = pIpPkt.ipHdr.checksum;
  pIpPkt.ipHdr.checksum = 0;

  //Compute checksum and compare with received value.
  if ((checksum != ::checksum(&pIpPkt.ipHdr, sizeof(pIpPkt.ipHdr))) && checksum != 0)
  {
    printf("NetIpRcv: bad checksum\n\r");
    return false; //Bad checksum
  }

  printf("NetIpRcv: Packet Received:\n\r");
  printf("NetIpRcv: Source Ip: ");
  printIp(&pIpPkt.ipHdr.src_addr);
  printf("NetIpRcv: Destination Ip: ");
  printIp(&pIpPkt.ipHdr.dest_addr);
  printf("NetIpRcv: Id: ");
  printf("NetIpRcv: Id: %d\r\n", pIpPkt.ipHdr.ident);

  if(pIpPkt.ipHdr.dest_addr == *((uint32_t*)srcIp)){
    printf("NetIpRcv: IP Pass\n\r");

    if(routingReceive(pIpPkt) == true){
        memcpy(data, pIpPkt.data, pIpPkt.ipHdr.length);
        *len = pIpPkt.ipHdr.length;
        return true;
    } else {
        return false;
    }

  } else {
    printf("NetIpRcv: IP Wrong\n\r");

    if(routingSend(pIpPkt) == true){
        printf("NetIpRcv: Resending\n\r");

        // delay para teste
        delay(50);
        NetIpSnd(pIpPkt);
    }

    return false;
  }



}

void printIp(uint32_t* ip){
  printf("%u.%u.%u.%u\n\r", ((uint8_t*)ip)[0], ((uint8_t*)ip)[1], ((uint8_t*)ip)[2], ((uint8_t*)ip)[3] );
}

void fillIp(uint32_t* destAddr, uint8_t* ipAddr){
  *destAddr = *((uint32_t*)ipAddr);
}




bool UdpSnd(uint16_t dest_port, void* data, uint16_t len){
  UdpPkt udpPacket;

  memcpy(&udpPacket.data, data, len);

  udpPacket.dest_port = dest_port;
  udpPacket.src_port = srcPort;
  udpPacket.length = len;
  udpPacket.checksum = 0;

  bool ok;
  ok = NetIpSnd((void*)&udpPacket, sizeof(udpPacket));

  if(ok){
    return true;
  } else {
    printf("UdpSend: Not ok\n\r");
    return false;
  }
}

bool UdpRcv(void* data, uint16_t* len){
  UdpPkt udpPacket;
  uint16_t dataSize;

  bool ok;
  ok = NetIpRcv((void*)&udpPacket, &dataSize);

  if(ok){
    if( udpPacket.dest_port == srcPort ){
      printf("UdpRcv: Port pass\n\r");
      //printf("%d\n\r",udpPacket.length);
      memcpy(data, udpPacket.data, udpPacket.length);
      *len = udpPacket.length;
      return true;
    } else {
      printf("UdpRcv: Port wrong\n\r");
      return false;
    }
  } else {
    //printf("UdpRcv: Not ok\n\r");
    return false;
  }

  return false;
}


#define SENT_BUFFER_SIZE 12
#define RECEIVE_BUFFER_SIZE SENT_BUFFER_SIZE

uint16_t sentBuffer[SENT_BUFFER_SIZE];
uint16_t sentBufferIndex = 0;
uint16_t receiveBuffer[RECEIVE_BUFFER_SIZE];
uint32_t receiveBufferIndex = 0;

/*
    Retorna false se o pacote já foi enviado e true caso contrário
*/
bool routingSend(NetIpPkt packet){
    int i;
    bool packetWasSent = false;

    for(i = 0; i<RECEIVE_BUFFER_SIZE; i++){
            if(packet.ipHdr.ident == sentBuffer[i]){
                packetWasSent = true;
            }
    }

    if(packetWasSent){
        printf("routingSend: Packet already sent\n\r");
        return false;
    } else {
        printf("routingSend: Packet new\n\r");
        sentBuffer[sentBufferIndex++] = packet.ipHdr.ident;
        if(sentBufferIndex >= SENT_BUFFER_SIZE){
            sentBufferIndex = 0;
        }
        return true;
    }
}

/*
    Retorna false se o pacote já foi recebido anteriormente e true caso contrário
*/
bool routingReceive(NetIpPkt packet){
    int i;
    bool packetWasReceived = false;

    for(i = 0; i<RECEIVE_BUFFER_SIZE; i++){
            if(packet.ipHdr.ident == receiveBuffer[i]){
                packetWasReceived = true;
            }
    }

    if(packetWasReceived){
        printf("routingReceive: Packet already received\n\r");
        return false;
    } else {
        printf("routingReceive: Packet new\n\r");
        receiveBuffer[receiveBufferIndex++] = packet.ipHdr.ident;
        if(receiveBufferIndex >= RECEIVE_BUFFER_SIZE){
            receiveBufferIndex = 0;
        }
        return true;
    }

}








void loop(void)
{

  // Ping out role.
  if (role == role_ping_out)
  {
    #ifdef CLIENT_NUMBER_1
    char stringToSend[] = "oi1";
    #endif
    #ifdef CLIENT_NUMBER_2
    char stringToSend[] = "oi2";
    #endif
    #ifdef CLIENT_NUMBER_3
    char stringToSend[] = "oi3";
    #endif
    #ifdef CLIENT_NUMBER_4
    char stringToSend[] = "oi4";
    #endif

    // TRANSMIT
    printf("Sending Message: %s\n\r", stringToSend);

    bool ok = UdpSnd(destPort, (uint8_t*)stringToSend, sizeof(stringToSend));

    if (ok)
      printf("Send ok...\n\r");
    else
      printf("Send failed.\n\r");




    // // RECEIVE ACK
    // char stringReceived[20];
    // uint16_t stringSize;
    // ok = UdpRcv(stringReceived, &stringSize);
    //
    // if(ok)
    //   printf("Message Received: %s\n\r", stringReceived);
    // else
    //   printf("Response Not Received\n\r");




    printf("\n\r");
    // Try again 1s later
    delay(1000);
  }

  // Pong back role.  Receive each packet, dump it out, and send it back
  if ( role == role_pong_back )
  {
    // RECEIVE
    char stringReceived[20];
    uint16_t stringSize;
    bool ok = UdpRcv(stringReceived, &stringSize);
    if (ok)
        printf("Message Received: %s\n\r", stringReceived);

    // Delay just a little bit to let the other unit
  	// make the transition to receiver
  	delay(1);




    // // TRANSMIT ACK
    // #ifdef CLIENT_NUMBER_1
    // char stringToSend[] = "si1";
    // #endif
    // #ifdef CLIENT_NUMBER_2
    // char stringToSend[] = "si2";
    // #endif
    // #ifdef CLIENT_NUMBER_3
    // char stringToSend[] = "si3";
    // #endif
    //
    // printf("Sending Message: %s\n\r", stringToSend);
    //
    // bool ok = UdpSnd(destPort, stringToSend, sizeof(stringToSend));
    //
    // if (ok)
    //   printf("Send ok...\n\r");
    // else
    //   printf("Send failed.\n\r\r");



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
    uint16_t * ptr = (uint16_t*)addr;

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
