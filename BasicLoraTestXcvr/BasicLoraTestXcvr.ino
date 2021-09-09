#include <SPI.h>
#include <Wire.h>
#include <SSD1306.h>
#include <LoRa.h>
#include "images.h"
#ifdef ESP32
  #include <WiFi.h>
#endif

#define VERSION "1.0.0"
/*
 * Started with the Transmitter and Receiver projects from https://github.com/YogoGit/TTGO-LORA32-V1.0.
 * Combined them together, to make transceivers that send and receive the test packets.  Among the
 * changes:
 * - replace String objects with char arrays to preclude possibility of memory leaks over the long term
 *   (e.g., 24+ hour runtime)
 * - modfied timing to allow receiving of a higher proportion of packets by any device (remember SX1276 
 *   only receives or tansmits at any one time, so we WILL miss some packets while transmitting).
 * - modified display to show both send and received stats simultaneously, updated both on packet
 *   transmit and receive (twice per loop).
 * - use random poll delay seeded by ESP32 WiFi MAC address, to make sure the two transceivers can't
 *   get stuck in lockstep.  Observationally, the timings I used here seem to result in 50-60% +/- an
 *   elephant packet loss over the long term
 * - uses the same two libraries referenced in the YoGoGit project
 * - TODO - Look int using receive callbacks instead of single receive to prevent packet trashing when
 *   a new packet comes in while doing a parsePacket()
 * - TODO - if new packet received during parsePacket(), may also be able to use to reduce packet loss if
 *   user lower transmission rate.
 * - TODO - when I have the time, modify this project or make a new project based on this that provides
 *   ack and retransmit so that no packets are lost.  Maybe change to a "Ping-Pong" pair if you will.
 *   nothing too fancy, this is just a test app, not a bullet-proof prod deployment.
 */

//#define LORA_BAND    433
//#define LORA_BAND    868
#define LORA_BAND    915

#define OLED_SDA    4
#define OLED_SCL    15
#define OLED_RST    16
#define MAX_PACKET_LEN 12
#define MAX_POLLS_PER_CYLE 50
#define POLL_DELAY_MIN 67
#define POLL_DELAY_MAX 83
#define MAX_XMIT_RETRIES 5
#define XMIT_RETRY_TIME  10
#define XMIT_LED_DELAY 40
#define NO_DATA_FLAG -999

#define SCK     5    // GPIO5  -- SX1278's SCK
#define MISO    19   // GPIO19 -- SX1278's MISO
#define MOSI    27   // GPIO27 -- SX1278's MOSI
#define SS      18   // GPIO18 -- SX1278's CS
#define RST     14   // GPIO14 -- SX1278's RESET
#define DI0     26   // GPIO26 -- SX1278's IRQ(Interrupt Request)

SSD1306 display(0x3c, OLED_SDA, OLED_SCL);

// Forward declarations
void showLogo();
void displayLoraData(int sentCounter, int rcvdCounter, int packSize, char* packet, int rssi);
int generatePollDelay();
void printMacAddrFromArray(byte macAddr[6]);

int rssi = NO_DATA_FLAG;
int packSize = NO_DATA_FLAG;
int sentCounter = 0;
int rcvdCounter = 0;
char dispBuffer[128]; 
char packet[MAX_PACKET_LEN+1];
long seed = 999;
unsigned long pollDelayTime;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println();
  Serial.print("LoRa Transceiver ");
  Serial.println(VERSION);

  // Configure the LED an an output
  pinMode(LED_BUILTIN, OUTPUT);

  // Configure OLED by setting the OLED Reset HIGH, LOW, and then back HIGH
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, HIGH);
  delay(100);
  digitalWrite(OLED_RST, LOW);
  delay(100);
  digitalWrite(OLED_RST, HIGH);

  display.init();
  display.flipScreenVertically();

  showLogo();
  delay(2000);

  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  char dispBuff[30];
  sprintf(dispBuff, "v%s", VERSION);
  display.drawString(display.getWidth() / 2, 22, "LoRa Transceiver");
  display.setFont(ArialMT_Plain_10);
  display.drawString(display.getWidth() / 2, 42, dispBuff);
  display.display();
  delay(2000);

#ifdef ESP32
  Serial.println("Generating random poll delay seed from MAC");
  Serial.println("ESP Board MAC Address:  ");
  seed = 0;
  byte wiFiMacAddr[6];
  WiFi.macAddress(wiFiMacAddr);
  for (int i=0; i < 6; i++) {
    seed += wiFiMacAddr[i];
  }
  Serial.println(WiFi.macAddress());
#endif
  
  Serial.print("Seeing random number generator, seed: ");
  Serial.println(seed);
  randomSeed(seed);
  pollDelayTime = (unsigned long)random(POLL_DELAY_MIN, POLL_DELAY_MAX);
  
  // Configure the LoRA radio
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(LORA_BAND * 1E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("init ok");

  sprintf(packet,"<none yet>");
}

void loop() {

  // send packet

  // toggle the led to give a visual indication the packet is being sent
  digitalWrite(LED_BUILTIN, HIGH);
  unsigned long int xmitTime = millis();
  Serial.println("Sending packet");

  int retries = 0;
  // retries probably not necessary in non-callback mode?
  while (retries < MAX_XMIT_RETRIES) {
    if (LoRa.beginPacket()) {
      break;
    }
    Serial.print("*** Retry xmit begin #");
    Serial.println(++retries);
    delay(XMIT_RETRY_TIME);
  }
  LoRa.print("hello ");
  LoRa.print(sentCounter);
  LoRa.endPacket();
  sprintf(dispBuffer, "Sent packet %d", sentCounter);
  Serial.println(dispBuffer);

  displayLoraData(sentCounter, rcvdCounter, packSize, packet, rssi);
  xmitTime = millis() - xmitTime;
  Serial.print("Xmit time (ms): ");
  Serial.println(xmitTime);
  if (xmitTime < XMIT_LED_DELAY) {
    delay(XMIT_LED_DELAY - xmitTime);
  }
  //delay(XMIT_LED_DELAY);
  digitalWrite(LED_BUILTIN, LOW);
  sentCounter++;

  //receive packet
  int rcvPollCount = 0;
  int packetSize = 0;
  // poll for received packet until get packet or reach max polls
  while ((packetSize == 0) && (rcvPollCount < MAX_POLLS_PER_CYLE)) {
    // Serial.println(rcvPollCount);
    packetSize = LoRa.parsePacket();
    if (packetSize) {
    Serial.print("Got packet of size ");
    Serial.print(packetSize);
    Serial.print(", on poll #");
    Serial.println(rcvPollCount);
    for (int i = 0; i < packetSize; i++) {
        if (i < (MAX_PACKET_LEN)) {
          packet[i] = (char)LoRa.read();
        }
        else {
          LoRa.read(); // into bit bucket
        }
      }
      if(packetSize > (MAX_PACKET_LEN)) {
        packetSize = MAX_PACKET_LEN;
      }
      packet[packetSize] = '\0';
      rssi = LoRa.packetRssi();
      sprintf(dispBuffer, "RSSI: %d", rssi);
      packSize = packetSize;
      rcvdCounter++;
      displayLoraData(sentCounter, rcvdCounter, packSize, packet, rssi);
    }
    rcvPollCount++;
    // want to delay a little after receipt as well, to let other side start
    // polling after xmit
    delay(pollDelayTime);
  }
  if (packetSize == 0) {
    Serial.println("No packet received during polls");
  }
  pollDelayTime = genPollDelay();  
  //delay(delayTime);
}

void displayLoraData(int sentCounter, int rcvdCounter, int packSize, char* packet, int rssi) {
  //Serial.println("Starting display");
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  sprintf(dispBuffer, "Sending pkt: %d", sentCounter);
  //Serial.println(dispBuffer);
  display.drawString(0, 0, dispBuffer);
  if (packSize == NO_DATA_FLAG) {
    sprintf(dispBuffer, "Last pkt rcvd bytes: <n/a>");
  }
  else {
    sprintf(dispBuffer, "Last pkt rcvd bytes: %d", packSize);
  }                            
  display.drawString(0 , 11, dispBuffer);
  //Serial.println(dispBuffer);
  sprintf(dispBuffer, "Total rcvd pkts: %d" , rcvdCounter);
  display.drawString(0 , 22, dispBuffer);
  //Serial.println(dispBuffer);
  sprintf(dispBuffer, "last pkt: \"%s\"", packet);
  display.drawStringMaxWidth(0 , 33 , 128, dispBuffer);
  //Serial.println(dispBuffer);
  if (rssi == NO_DATA_FLAG) {
    sprintf(dispBuffer, "Last RSSI: <n/a>      v%s", VERSION);
  }
  else {
     sprintf(dispBuffer, "Last RSSI: %d        v%s", rssi, VERSION);
  }                     
  display.drawString(0, 44, dispBuffer);
  //Serial.println(dispBuffer);
  display.display();
  //Serial.println("Ending display");
}

void showLogo() {
  uint8_t x_off = (display.getWidth() - logo_width) / 2;
  uint8_t y_off = (display.getHeight() - logo_height) / 2;

  display.clear();
  display.drawXbm(x_off, y_off, logo_width, logo_height, logo_bits);
  display.display();
}

unsigned long genPollDelay() {
  unsigned long pollDelayTime = (unsigned long)random(POLL_DELAY_MIN, POLL_DELAY_MAX);
  Serial.print("Next poll delay time (ms): ");
  Serial.println(pollDelayTime);
  return pollDelayTime;
}

/* for debugging.
void printMacAddrFromArray(byte macAddr[6]) {
  char addrByteStr[8] {'M', 'A', 'C', '[', 0x00, ']', ':', 0x00};
  Serial.println();
  for (int i=0; i < 6; i++) {
    addrByteStr[4] = (byte)i + 48;
    Serial.print (addrByteStr);
    Serial.println(macAddr[i]);
  }
}
*/
