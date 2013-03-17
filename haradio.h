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

class HARadio
{
public:
    HARadio();
    void reset();
    void txBuffer(const uint8_t *buffer, uint8_t len);
    uint8_t rxBuffer(uint8_t *buffer, uint8_t len);
    
    int HARadio::setup_packet(struct packet *p);
    int HARadio::receive_packet(struct packet *p, uint8_t *buffer);
    int HARadio::send_packet(struct packet *p);

private:

    void spiTable(const prog_uchar *table);
    byte regRead(byte reg);
    byte statusRead();
    void DEBUGREG();
};

#endif

