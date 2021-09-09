# lora-test
## LoRa and LoRaWAN client test applications

Various test apps for LoRa testing, and LoRaWAN Things Stack (TTN v3) clients.  For now, only Arduino IDE-based projects.

### Projects
* Simple LoRa test transceiver
  * tested with LilyGo TTGO ESP32 LoRa 915MHz, with OLED display
  * Started with the Transmitter and Receiver projects from https://github.com/YogoGit/TTGO-LORA32-V1.0.
   Combined them together, to make transceivers that send and receive the test packets.  Among the
   changes:
    * replace String objects with char arrays to preclude possibility of memory leaks over the long term,
	  limit packet read into char array to the size of the array, etc. Tested stable for 24+ hour 	  runtimes.
    * use random poll delay seeded by ESP32 WiFi MAC address, to make sure the two transceivers don't
      get stuck in lockstep.  Observationally, the timings I used here seem to result in 50-60% +/- packet loss over the long term.  Something the pure transmitter/receiver pair of the 
	  original app won't experience (remember the SX1276 only receives or tansmits at any one time, 
	  so we WILL miss some packets while it's not in receive mode)
    * modified display to show both send and received stats simultaneously, updated both on packet
      transmit and receive (twice per loop).
    * uses the same two libraries referenced in the YoGoGit project
