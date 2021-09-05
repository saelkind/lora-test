# lora-test
## LoRa and LoRaWAN client test applications

Various test apps for LoRa testing, and LoRaWAN Things Stack (TTN v3) clients.  For now, only Arduino IDE-based projects.

### Projects
* Simple LoRa test transceiver
 * tested with LilyGo TTG ESP32 LoRa 915MHz, with OLED display
 *Started with the Transmitter and Receiver projects from https://github.com/YogoGit/TTGO-LORA32-V1.0.
 * Combined them together, to make transceivers that send and receive the test packets.  Among the
   changes:
  * replace String objects with char arrays to preclude possibility of memory leaks over the long term
  (e.g., 24+ hour runtime)
  * modfied timing to allow receiving of the majority of packets by any device (remember SX1276 only
  receives or tansmits at any one time, so we WILL miss some packets while transmitting).
  * modified display to show both send and received stats simultaneously, updated both on packet
  transmit and receive (twice per look).
  * uses the same two libraries referenced in the YoGoGit project
