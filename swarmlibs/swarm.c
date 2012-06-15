#include "../gen/r_cg_macrodriver.h"
#include "../redpine/rsi_data_types.h"
#include "../redpine/rsi_uart_api.h"
#include "../redpine/rsi_hal.h"
#include "../redpine/rsi_global.h"
#include "../redpine/rsi_hal_api.h"
//#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

//TODO - both these headers for whatever reason need an extra \r\n.... BAHTWHY?
const char produce_header[] = "POST /stream?swarm_id=%s&"
  "resource_id=%s HTTP/1.1\r\nHost:api.bugswarm.com\r\n"
  "x-bugswarmapikey:%s\r\ntransfer-encoding:chunked\r\nConnection:keep-alive"
  "\r\nContent-Type: application/json\r\n\r\n1\r\n\n\r\n\r\n";
//const char feed_request[] = "{\"capabilities\": {\"feeds\": [\"Acceleration\",\"Light\"], \"modules\": {\"slot1\":\"LCD\"}}}";
const char feed_request[] = "{\"capabilities\": {\"feeds\": [\"Acceleration\",\"Temperature\",\"Button\",\"Light\",\"LED\",\"Potentiometer\"]}}";
const char message_header[] = "%x\r\n{\"message\": {\"payload\":%s}}\r\n\r\n\r\n";

char pktbuff[500];
int pktlen;

void swarm_init(){
  
}

void swarm_send_produce(const char *swarm_id, const char *resource_id, const char *part_key, struct rsi_socketFrame_s *sock){
  memset(pktbuff, '\0', sizeof(pktbuff));
  sprintf(pktbuff, produce_header, swarm_id, resource_id, part_key);
  sock->buf = (uint8 *)pktbuff;
  sock->buf_len = strlen((const char *)sock->buf);
  rsi_send(sock);
}

void swarm_produce(char *message, struct rsi_socketFrame_s *sock){
  memset(pktbuff, '\0', sizeof(pktbuff));
  pktlen = strlen(message)+sizeof(message_header)-11;
  sprintf(pktbuff,message_header,pktlen,message);
  sock->buf = (uint8 *)pktbuff;
  sock->buf_len = strlen((const char *)sock->buf);
  rsi_send(sock);
}

void capabilities_announce(struct rsi_socketFrame_s *sock){
  swarm_produce((char *)feed_request, sock);
}