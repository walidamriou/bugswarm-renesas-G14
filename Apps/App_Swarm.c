/*-------------------------------------------------------------------------*
 * File:  App_Swarm.c
 *-------------------------------------------------------------------------*
 * Description:
 *     Routines for connecting to bugswarm.  
 *-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*
 * Includes:
 *-------------------------------------------------------------------------*/
//TODO - remove unused imports, this was copypasta
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <HostApp.h>
#include <system/platform.h>
#include <system/console.h>
#include <sensors/Accelerometer.h>
#include <sensors/LightSensor.h>
#include <sensors/Temperature.h>
#include <sensors/Potentiometer.h>
#include <CmdLib/AtCmdLib.h>
#include <CmdLib/GainSpan_SPI.h>
#include <system/mstimer.h>
#include <system/Switch.h>
#include <drv/Glyph/lcd.h>
#include "Apps.h"
#include <jsmn/jsmn.h>

#define UPDATE_PERIOD 1000
#define NUM_SENSOR 3
#define CAPABILITIES_PERIOD 10000

/*-------------------------------------------------------------------------*
 * Constants:
 *-------------------------------------------------------------------------*/
uint16_t period = NUM_SENSOR;
//const char SwarmHost[] = "192.168.11.231";
const char SwarmHost[] = "107.20.250.52";  //api.bugswarm.net
char resource_id[] = "14f762c59815e24973165668aff677659b973d62";
const char swarm_id[] = "69df1aea11433b3f85d2ca6e9c3575a9c86f8182";
const char participation_key[] = "bc60aa60d80f7c104ad1e028a5223e7660da5f8c";
const char configuration_key[] = "359aff0298658552ec987b9354ea754b684a4047";
const char password_hash[] = "cmVuZXNhczpyZW5lc2FzcHNr";

const char configure_resource_header[] = "POST /renesas/configure HTTP/1.1\r\n"
    "Host: api.bugswarm.net\r\nAccept: */*\r\nx-bugswarmapikey: %s\r\n"
    "content-type: application/json\r\nContent-Length: %d\r\n\r\n%s\r\n";
const char configure_resource_body[] = 
    "{\"name\":\"%s\",\"swarmid\":\"%s\",\"description\":"
    "\"an RL78G14 board running bugswarm-renesasG14 1.0\"}";
const char list_resources_header[] = "GET /resources HTTP/1.1\r\n"
	"Host: api.bugswarm.net\r\nAccept: */*\r\nx-bugswarmapikey: %s\r\n\r\n";
const char apikey_header[] = "GET /keys/configuration HTTP/1.1\r\n"
"Authorization: Basic %s\r\nHost: api.bugswarm.net\r\nAccept: */*\r\n\r\n";
const char produce_header[] = "POST /stream?swarm_id=%s&"
  "resource_id=%s HTTP/1.1\r\nHost:api.bugswarm.com\r\n"
  "x-bugswarmapikey:%s\r\ntransfer-encoding:chunked\r\nConnection:keep-alive"
  "\r\nContent-Type: application/json\r\n\r\n1\r\n\n\r\n";
const char feed_request[] = "{\"capabilities\": {\"feeds\": [\"Acceleration\","
  "\"Temperature\",\"Button\",\"Light\",\"Sound Level\",\"Potentiometer\"]}}";
const char message_header[] = "%x\r\n{\"message\": {\"payload\":%s}}\r\n\r\n";

/*-------------------------------------------------------------------------*
 * Types:
 *-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*
 * Prototypes:
 *-------------------------------------------------------------------------*/
char    MACaddr[20];
char    MACstr[13];
char    pkt[1500];
char 	msg[512];
bool    connected;
ATLIBGS_TCPMessage tcp_pkt;

void App_RunConnector(void)
{
	ATLIBGS_MSG_ID_E r;
	uint8_t cid = 0;

	while(1) {
		r = AtLibGs_GetMAC(MACaddr);
		if(r != ATLIBGS_MSG_ID_OK) {
			DisplayLCD(LCD_LINE6, "Get MAC Failed!");
			MSTimerDelay(2000);
			continue;
			} 
		break;
	}; 
  
	if(r == ATLIBGS_MSG_ID_OK)
		AtLibGs_ParseGetMacResponse(MACstr);    
	ConsolePrintf("Module MAC Address: %s\r\n", MACstr);
        
	connected = false;
	while(1){
		if (!AtLibGs_IsNodeAssociated()) {
			ConsolePrintf("Connecting to %s...",G_nvsettings.webprov.ssid);
			r = App_Connect(&G_nvsettings.webprov);
			ConsolePrintf(" %d\r\n",r);        
			if (r != ATLIBGS_MSG_ID_OK) {
				DisplayLCD(LCD_LINE4, "WIFI ERR: RESET");
				MSTimerDelay(1000);
				void (*reset) (void) = (void (*)(void)) (0xFFFF);
			}
			//TODO - verify connection attempt against enum
		}
		DisplayLCD(LCD_LINE3, "Obtaining Swarm ID");
                
		//r = getAPIKey(pkt, sizeof(pkt), configuration_key);    
		r = getResourceID(MACstr, pkt, sizeof(pkt), resource_id);
		if (r != ATLIBGS_MSG_ID_OK){
			DisplayLCD(LCD_LINE3, "ID: UnknownDevice");
		} else {
			sprintf(msg, "ID: %s",MACstr);
			DisplayLCD(LCD_LINE3, msg);
		}
		
		//TODO DELETEME!
		//ConsolePrintf("Test complete, quit\r\n");
		//return;
        MSTimerDelay(250);
		ConsolePrintf("Creating a production session\r\n");
		DisplayLCD(LCD_LINE8, "Opening Connection");
		r = createProductionSession(&cid, 
                                         (char *)SwarmHost,
                                         (char *)swarm_id,
                                         (char *)resource_id,
                                         (char *)participation_key);
   		if (r > 1){
			ConsolePrintf("Unable to open production session, retrying\r\n");
			DisplayLCD(LCD_LINE8, "Couldn't open socket");
			connected = false;
			DisplayLCD(LCD_LINE4, "SOCKET ERR: RESET");
			MSTimerDelay(2000);
			void (*reset) (void) = (void (*)(void)) (0xFFFF);
			//continue;
		}
		connected = true;
		while(connected) {
			DisplayLCD(LCD_LINE8, "Connected!");
			r = App_SwarmProducer(cid);
			if (r > 1){
				ConsolePrintf("Error producing to swarm, retrying\r\n");
				DisplayLCD(LCD_LINE8, "Production err");
				DisplayLCD(LCD_LINE4, "PROD ERR: RESET");
				MSTimerDelay(2000);
				void (*reset) (void) = (void (*)(void)) (0xFFFF);
				connected = false;
				//continue;
			} else if (r == 0) {
				ConsolePrintf("Production session complete\r\n");
				return;
			}
		}
	}  
}

ATLIBGS_MSG_ID_E getResourceID (char * mac_addr_str, char * buff, int bufflen, char * result){
	ATLIBGS_MSG_ID_E r;
	uint8_t cid = 0;
	int len;
//	jsmn_parser parser;
//	jsmntok_t tokens[20];
//	jsmnerr_t jr;
//	
//	jsmn_init(&parser);
//        const char configure_resource_header[] = "POST /renesas/configure HTTP/1.1\r\n"
//            "Host: api.bugswarm.net\r\nAccept: */*\r\nx-bugswarmapikey: %s\r\n"
//            "content-type: application/json\r\nContent-Length: %d\r\n\r\n%s\r\n";
//        const char configure_resource_body[] = 
//            "{\"name\":\"%s\",\"swarmid\":\"%s\",\"description\":"
//            "\"an RL78G14 board running bugswarm-renesasG14 1.0\"}";
        
	//r = makeAPICall(&cid, buff, list_resources_header, configuration_key);
	len = sprintf(msg, configure_resource_body, mac_addr_str, swarm_id);
	r = makeAPICall(&cid, buff, configure_resource_header, 
					configuration_key, len, msg);
	if (r != ATLIBGS_MSG_ID_OK) {
        ConsolePrintf("Unable to make API call:\r\n%s",buff);
		return r;
	}
 
	r = readOnePacket(buff, bufflen, &len, 5000);
	if (r != ATLIBGS_MSG_ID_OK){
		ConsolePrintf("Error retrieving response %d\r\n", r);
		DisplayLCD(LCD_LINE8, "swarm readcfg err");
		return r;
	}
	ConsolePrintf("Got %d bytes\r\n",len);
	char * respos = strstr(buff, "\r\n\r\n")+4;
	if (strlen(respos) == 40) {
		memcpy(resource_id, respos, 40);
	} else {
		ConsolePrintf("Invalid resource id from server, using default "
					  "\"UnknownDevice\"\r\n");
		DisplayLCD(LCD_LINE8, "invalid cfg resp");
	}
	ConsolePrintf("Using resource id: %s\r\n",resource_id);
//	AtLibGs_Close(cid);
//	char * jsonpos = strstr(buff, "\r\n\r\n")+4;
//	if (jsonpos >= buff+strlen(buff)){
//		ConsolePrintf("Could not find a body to the response: %s\r\n", buff);
//		return ATLIBGS_MSG_ID_INVALID_INPUT;
//	}
//	ConsolePrintf("APIkey JSON: %s\r\n",jsonpos);
//	jr = jsmn_parse(&parser, jsonpos, tokens, 20);
//	if (jr != JSMN_SUCCESS){
//		ConsolePrintf("Error parsing json: %d\r\n", jr);
//		return ATLIBGS_MSG_ID_INVALID_INPUT;
//	}
//	
//	r = ATLIBGS_MSG_ID_INVALID_INPUT;
//	
//	for (int i=0;i<20;i++){
//		ConsolePrintf("  Token type: %d, Size: %d, Range: (%d-%d)\r\n",
//					  tokens[i].type, tokens[i].size, tokens[i].start, tokens[i].end);
//	}
//	for (int i=0;i<20;i++){
//		if ((tokens[i].type == 3) && (tokens[i].size == 0) && 
//					(tokens[i].end-tokens[i].start == 3)){
//			if (strncmp(jsonpos + tokens[i].start, "key", 3) == 0){
//				ConsolePrintf("Found APIKEY \"%.40s\"\r\n",jsonpos+tokens[i+1].start);
//				r = ATLIBGS_MSG_ID_OK;
//				break;
//			}
//		} 
//	}
	
	return r;
}

ATLIBGS_MSG_ID_E getAPIKey(char * buff, int bufflen, char * result){
	ATLIBGS_MSG_ID_E r;
	uint8_t cid = 0;
	int len;
	jsmn_parser parser;
	jsmntok_t tokens[20];
	jsmnerr_t jr;
	
	jsmn_init(&parser);
	
	r = makeAPICall(&cid, buff, apikey_header, password_hash);
	if (r != ATLIBGS_MSG_ID_OK) {
		ConsolePrintf("Unable to make API call\r\n");
		return r;
	}
 
	r = readOnePacket(buff, bufflen, &len, 5000);
	if (r != ATLIBGS_MSG_ID_OK){ 
		ConsolePrintf("Error retrieving response %d\r\n", r);
		return r;
	}
	ConsolePrintf("Got %d bytes\r\n",len);
	AtLibGs_Close(cid);
	char * jsonpos = strstr(buff, "\r\n\r\n")+4;
	if (jsonpos >= buff+strlen(buff)){
		ConsolePrintf("Could not find a body to the response: %s\r\n", buff);
		return ATLIBGS_MSG_ID_INVALID_INPUT;
	}
	ConsolePrintf("APIkey JSON: %s\r\n",jsonpos);
	jr = jsmn_parse(&parser, jsonpos, tokens, 20);
	if (jr != JSMN_SUCCESS){
		ConsolePrintf("Error parsing json: %d\r\n", jr);
		return ATLIBGS_MSG_ID_INVALID_INPUT;
	}
	
	r = ATLIBGS_MSG_ID_INVALID_INPUT;
	
	for (int i=0;i<20;i++){
		if ((tokens[i].type == 3) && (tokens[i].size == 0) && 
					(tokens[i].end-tokens[i].start == 3)){
			if (strncmp(jsonpos + tokens[i].start, "key", 3) == 0){
				ConsolePrintf("Found APIKEY \"%.40s\"\r\n",jsonpos+tokens[i+1].start);
				strncpy(result, jsonpos+tokens[i+1].start, 40);
				r = ATLIBGS_MSG_ID_OK;
				break;
			}
		} 
	}
	if (r == ATLIBGS_MSG_ID_INVALID_INPUT){
		ConsolePrintf("Unable to find 'key' in response\r\n");
	}
	
	return r;
}

ATLIBGS_MSG_ID_E makeAPICall(uint8_t * cid, char * buff, const char *format, ...){
	ATLIBGS_MSG_ID_E r;
	int len;
	va_list args;
    va_start(args, format);
	
	r = AtLibGs_TCPClientStart((char *)SwarmHost, 80, cid);
	ConsolePrintf("Opened a socket:  %d,%d\r\n",r,*cid);
	if ((r != ATLIBGS_MSG_ID_OK) || (*cid == ATLIBGS_INVALID_CID)){
		ConsolePrintf("Unable to connect to TCP socket\r\n");
		DisplayLCD(LCD_LINE8, "API socket err");
		return r;
	}
	App_PrepareIncomingData();      
	ConsolePrintf("Socket open, sending headers\r\n");
  
	len = vsprintf((char *)buff, format, args);
	//ConsolePrintf("Attempting to send: %s", buff);
	r = AtLibGs_SendTCPData(*cid, (uint8_t *)buff, len);
	//ConsolePrintf("Sent headers: %d\r\n", r);
	if (r != ATLIBGS_MSG_ID_OK) {
		ConsolePrintf("Error transmitting headers to server (%d)%s\r\n", r, buff);
		DisplayLCD(LCD_LINE8, "API send err");
		return r;
	}
	return ATLIBGS_MSG_ID_OK;
}
						
ATLIBGS_MSG_ID_E App_SwarmProducer(uint8_t cid) {
	ATLIBGS_MSG_ID_E r;
	uint16_t value;
	uint16_t mic_level;
	float temp, tempF;
	extern int16_t	gAccData[3];
	int idx = 0;
	
	while(1) {
		ConsolePrintf("Temp: ");
		value = Temperature_Get();
                //2.5 is an approximate measure of board self-heating...
		temp = (((float)value)/128.0)-2.5;  
	        //tempF = ((temp/5)*9)/128 + 22;
                tempF = (temp*1.8)+32.0;
		ConsolePrintf("%0.1fC %0.1fF\r\n", temp, tempF);
		r = produce(cid, "{\"name\":\"Temperature\",\"feed\":{\"TempF\":%0.2f}}",
					tempF);
		if (r > 1)
			return r;
		readForAtLeast(cid, 200);
		
		ConsolePrintf("Light: ");
		value = LightSensor_Get();
		ConsolePrintf("%u\r\n", value);
		r = produce(cid, "{\"name\":\"Light\",\"feed\":{\"Value\":%u}}",
					value);
		if (r > 1)
			return r;
		sprintf(msg, "T: %0.2fF, L: %u",tempF, value);
		DisplayLCD(LCD_LINE5, msg);
		readForAtLeast(cid, 200);
		
		ConsolePrintf("Accel: ");
		Accelerometer_Get();
		ConsolePrintf("(%d, %d, %d)\r\n", gAccData[0], gAccData[1], gAccData[2]);	
		r = produce(cid, "{\"name\":\"Acceleration\","
					"\"feed\":{\"x\":%0.2f,\"y\":%0.2f,\"z\":%0.2f}}",
					(float)gAccData[0]/33.0, (float)gAccData[1]/33.0, (float)gAccData[2]/30.0);
		if (r > 1)
			return r;
		sprintf(msg, "A(%0.2f,%0.2f,%0.2f)",
				(float)gAccData[0]/33.0, (float)gAccData[1]/33.0, (float)gAccData[2]/30.0);
		DisplayLCD(LCD_LINE6, msg);
		readForAtLeast(cid, 200);
		
		ConsolePrintf("Mic: ");
		value = Microphone_Get();
		value = Microphone_Get();	
		mic_level = abs((int)(value/4)-493);
		ConsolePrintf("%u, %04x\r\n", mic_level, value);		
		r = produce(cid, "{\"name\":\"Sound Level\",\"feed\":{\"Raw\":%u}}",
					mic_level);
		if (r > 1)
			return r;
		readForAtLeast(cid, 200);		
		
		ConsolePrintf("Pot: ");
		value = Potentiometer_Get();
		value = Potentiometer_Get();
		ConsolePrintf("%u, %04x\r\n", value/4, value);
		r = produce(cid, "{\"name\":\"Potentiometer\",\"feed\":{\"Raw\":%u}}",
					value/4);
		if (r > 1)
			return r;
		sprintf(msg, "P: %u M: %u", value/4, mic_level);
		DisplayLCD(LCD_LINE7, msg);
		readForAtLeast(cid, 200);
		
		if (idx++%5 == 0){
           produce(cid, feed_request);
        }
	}
}

ATLIBGS_MSG_ID_E produce(uint8_t cid, const char *format, ...) {
	va_list args;
	int len;

	va_start(args, format);
	len = vsprintf((char *)msg, format, args);
	
	len = sprintf(pkt, message_header, len+sizeof(message_header)-9, msg);
	//ConsolePrintf("Sending: *%s*", pkt);
	return AtLibGs_SendTCPData(cid, (uint8_t *)pkt, len);
}

ATLIBGS_MSG_ID_E createProductionSession(uint8_t *cid, 
                                         char * hostIP,
                                         char * swarm_id,
                                         char * resource_id,
                                         char * participation_key) {
	ATLIBGS_MSG_ID_E r;
	int len;
	
	if (!AtLibGs_IsNodeAssociated()) {
		ConsolePrintf("Connecting to %s...",G_nvsettings.webprov.ssid);
		r = App_Connect(&G_nvsettings.webprov);
		ConsolePrintf(" %d\r\n",r);
		//TODO - verify connection attempt against enum
	}
	ConsolePrintf("Attempting to connect to %s:%d ...",hostIP, 80);
	r = AtLibGs_TCPClientStart(hostIP, 80, cid);
	ConsolePrintf(" %d,%d\r\n",r,cid);
	if ((r != ATLIBGS_MSG_ID_OK) || ((*cid) == ATLIBGS_INVALID_CID)){
		ConsolePrintf("Unable to connect to TCP socket\r\n");
		DisplayLCD(LCD_LINE8, "produce socket err");
		return r;
	}
	App_PrepareIncomingData();      
	ConsolePrintf("Socket open, sending headers\r\n");
  
	len = sprintf(pkt, produce_header, swarm_id, resource_id, participation_key);
	ConsolePrintf("Attempting to send: %s ...", pkt);
	r = AtLibGs_SendTCPData((*cid), (uint8_t *)pkt, len);
	ConsolePrintf(" %d\r\n", r);
	if (r != ATLIBGS_MSG_ID_OK) {
		ConsolePrintf("Error transmitting headers to server\r\n");
		DisplayLCD(LCD_LINE8, "produce header err");
		return r;
	}
	DisplayLCD(LCD_LINE8, "waiting for presence");
 
	//Wait up to 5 seconds for presence messages.
	readForAtLeast(*cid, 5000);
	
	ConsolePrintf("Done waiting for presence, connected to swarm!\r\n");
	return ATLIBGS_MSG_ID_OK;
}

ATLIBGS_MSG_ID_E readOnePacket(char * buff, int len, int * written, uint32_t ms){
	ATLIBGS_MSG_ID_E r;
	
	r = AtLibGs_WaitForTCPMessage(ms);
	if (r != ATLIBGS_MSG_ID_DATA_RX) {
		ConsolePrintf("Issue retrieving response: %u\r\n", r);
		return r;
	}
	AtLibGs_ParseTCPData(G_received, G_receivedCount, &tcp_pkt);
	//ConsolePrintf("*(%u)%s*",tcp_pkt.numBytes,tcp_pkt.message);                
	if (tcp_pkt.numBytes >= len){
		tcp_pkt.numBytes = len-1;
		ConsolePrintf("WARN: message greater than %d bytes, concatenated\r\n",len);
	}
	memset(buff, '\0', tcp_pkt.numBytes+1);
	memcpy(buff, tcp_pkt.message, tcp_pkt.numBytes);
	*written = tcp_pkt.numBytes;
	App_PrepareIncomingData();      
	return ATLIBGS_MSG_ID_OK;
}

void readForAtLeast(uint8_t cid, uint32_t ms){
  	uint32_t end = MSTimerGet()+ms;
	char * switches;
	//WHILE: bytes are waiting in buffer, WaitForTCPMessage has a new message,
	//		or it hasn't been ms milliseocnds:
	while ((G_receivedCount) || 
		   (AtLibGs_WaitForTCPMessage(10) == ATLIBGS_MSG_ID_DATA_RX) ||
		   (MSTimerGet() < end)){
		//If no bytes are waiting in the buffer, don't attempt to parse.
		if (!G_receivedCount) {
			if (checkSwitches(switches)) {
				ConsolePrintf("Switch Update Sw1: %u * SW2: %u * SW3: %u\r\n",Switch1IsPressed(), Switch2IsPressed(), Switch3IsPressed());
				produce(cid, "{\"name\":\"Button\",\"feed\":{\"b1\":%u,\"b2\":%u,\"b3\":%u}}",
					Switch1IsPressed(),Switch2IsPressed(),Switch3IsPressed());
			}
		  	continue;
		}
		AtLibGs_ParseTCPData(G_received, G_receivedCount, &tcp_pkt);
		//ConsolePrintf("*(%u)%s*",tcp_pkt.numBytes,tcp_pkt.message);                
		if (tcp_pkt.numBytes >= sizeof(pkt)){
			tcp_pkt.numBytes = sizeof(pkt)-1;
			ConsolePrintf("WARN: message greater than %d bytes, concatenated\r\n",sizeof(pkt));
		}
		memset(pkt, '\0', sizeof(pkt));
		memcpy(pkt, tcp_pkt.message, tcp_pkt.numBytes);
		ConsolePrintf("*(%u)%s*",tcp_pkt.numBytes,pkt);
		App_PrepareIncomingData();
	} 
}
