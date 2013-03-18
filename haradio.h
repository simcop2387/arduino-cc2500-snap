/*
 * Arduino library for emulating the Lelo vibrator remote.
 * Requires a CC2500 radio attached via SPI.
 *
 * Micah Elizabeth Scott <beth@scanlime.org>
 */

#ifndef haradio_h
#define haradio_h

#include "Arduino.h"
#include <avr/pgmspace.h>

struct packet {
  uint8_t sync; // do i need this? probably not but it's not a bad idea
  uint8_t HDB2;
  uint8_t HDB1;
  uint32_t target_address;
  uint32_t source_address;
  
  uint8_t *data;
  uint8_t dlength;
  uint8_t rlength;  // real length, for protocol
  // uint32_t pflags // protocol flags?  Do I want this?
};

class HARadio
{
public:
    HARadio(uint32_t address, int _csn = 10) : my_address( address ), csn ( _csn ) {};
    void reset();
    void txBuffer(const uint8_t *buffer, uint8_t len);
    uint8_t rxBuffer(uint8_t *buffer, uint8_t len);
    
    int setup_packet(struct packet *p);
    int receive_packet(struct packet *p, uint8_t buffer[64+9]);
    int send_packet(struct packet *p);

private:

    uint32_t my_address;
    int csn;
    
    void spiTable(const prog_uchar *table);
    byte regRead(byte reg);
    byte statusRead();
    void DEBUGREG();
};

#endif

