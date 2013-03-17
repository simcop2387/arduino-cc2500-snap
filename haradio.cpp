/*
 * Arduino library for emulating the Lelo vibrator remote.
 * Requires a CC2500 radio attached via SPI.
 *
 * Micah Elizabeth Scott <beth@scanlime.org>
 */

#include <SPI.h>
#include "haradio.h"

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

int HARadio::setup_packet(struct packet *p) {
  p->sync = 0b01010100; // required sync word
  p->source_address = my_address;
  uint8_t dlength = p->dlength;
  
  p->HDB2 = ((p->target_address & 0x00FF0000 ? 3 :   // Calculate how many bytes we need to use for the address, 3 for 24bit
              p->target_address & 0x0000FF00 ? 2 :   // 2 for 16 bit, 1 for 8 bit, or 0 for broadcast!
              p->target_address & 0x000000FF ? 1 : 0) << 6) |
            ((p->source_address & 0x00FF0000 ? 3 :   // Do the same for the source address, but always have at least one byte of it, no sending from 0, yet
	      p->source_address & 0x0000FF00 ? 2 : 1) << 4) | // TODO once I have this working I might use the command bit and protocol-bufs to handle internal details, in which case i'll do automatic address assignment, which will mean I want to be able to send from 0, or i'll use 255!
	    ((0) << 2)  | // Protocol specific flags, not defined in standard yet, but reserved for future use
	    (0)           // no ACKs needed, I won't be resending packets yet.  not enough ram on the uCs to do that well
	    ;
  p->HDB1 = (0 << 7) | // Command mode bit, we won't be using yet
            (0 << 4) | // Error detection/correct mode, none, TODO implement CRC32;
	    (dlength >= 0  && dlength <= 8  ? dlength :
	     dlength >  8  && dlength <= 16 ? 0b1001 :
     	     dlength >  16 && dlength <= 32 ? 0b1010 :
     	     dlength >  32 && dlength <= 64 ? 0b1011 : 0b1111 // use 0b1111 as an error, since I can't send more than 64 bytes right now
	    );

  p->rlength = (dlength >= 0  && dlength <= 8  ? dlength :
	        dlength >  8  && dlength <= 16 ? 16 :
     	        dlength >  16 && dlength <= 32 ? 32 :
     	        dlength >  32 && dlength <= 64 ? 64 : 0xFF // use 0xFF as an error, since I can't send more than 64 bytes right now
	       );
  
  return p->rlength;
}

int HARadio::receive_packet(struct packet *p, uint8_t *buffer) {
  int p_buff = 0;
  uint8_t dab, sab, ndb;
  
  p->sync = buffer[p_buff++];
  if (buffer[0] != 0b01010100)
    return -1;
  
  p->HDB2 = buffer[p_buff++];
  if ((p->HDB2 & 0b00110000 == 0x00) // Check for invalid source address bits for our network.  
   && (p->HDB2 & 0b00001100 != 0x00) // Does it have protocol flags? if so, throw it out
   && (p->HDB2 & 0b00000011 != 0x00)) // We don't support ACK right now, throw them out
  return -2;
  
  p->HDB1 = buffer[p_buff++];
  if ((p->HDB1 & 0b10000000) // Do we have command bit, we shouldn't
   && (p->HDB1 & 0b01110000) // Do we have error detection? we shouldn't
   && (p->HDB1 & 0b00001111 == 0xF) // Did we get a packet with user defined length? ignore it!
  return -3;
  
  ndb = p->HDB1 & 0b00001111;
  p->rlength = (ndb <= 0b1000 ? ndb :
                ndb == 0b1001 ? 16  :
                ndb == 0b1010 ? 32  : 64
  );
  
  dab = (p->HDB2 & 0b11000000) >> 6;
  if (dab == 0)
    p->target_address = 0; // Broadcast packet!
  else if (dab == 1) { // one byte
    p->target_address = buffer[p_buff];
  } else if (dab == 2) { // two bytes
    p->target_address = buffer[p_buff] << 8 |
                        buffer[p_buff+1];
  } else { // three bytes! wowza!
    p->target_address = buffer[p_buff]   << 16 |
                        buffer[p_buff+1] << 8  |
                        buffer[p_buff+2];
  }
  
  p_buff += dab;
  if (!(p->target_address == 0 ||
        p->target_address != my_address))
  return 0;  // not an error but not our packet
  
  sab = (p->HDB2 & 0b00110000) >> 4;
  if (sab == 1) { // Same deal as DAB above, pull off the bytes as needed, but SAB can't be empty
    p->source_address = buffer[p_buff];
  } else if (sab == 2) {
    p->source_address = buffer[p_buff] << 8 |
                        buffer[p_buff+1];
  } else {
    p->source_address = buffer[p_buff]   << 16 |
                        buffer[p_buff+1] << 8  |
                        buffer[p_buff+2];
  }
  
  p_buff += sab;
  
  // no such thing as an invalid source address right now, might have to have them later?
  // At this point we have the entire packet header parsed, we now have a pointer to the data, what do we do now?
  return p_buff;  // Give back the position in the buffer, so that the data can be read
}

int HARadio::send_packet(struct packet *p) {
  uint8_t buffer[64 + 9]; // buffer, 64 bytes for data, and 9 for header.  TODO will actually fail when sending 64 bytes!
  uint16_t p_buff = 0;
  
  if (p->sync != 0b01010100)
    return -1; // don't send if it doesn't have the sync byte!
  
  buffer[p_buff++] = p->sync;
  buffer[p_buff++] = p->HDB2;
  buffer[p_buff++] = p->HDB1;
  
  
  if (p->target_address & 0x00FF0000) {
    buffer[p_buff++] = (p->target_address & 0xFF0000) >> 16;
    buffer[p_buff++] = (p->target_address & 0x00FF00) >> 8;
    buffer[p_buff++] = (p->target_address & 0x0000FF);
  } else if (p->target_address & 0x0000FF00) {
    buffer[p_buff++] = (p->target_address & 0x00FF00) >> 8;
    buffer[p_buff++] = (p->target_address & 0x0000FF);
  } else if (p->target_address & 0x000000FF) {
    buffer[p_buff++] = (p->target_address & 0xFF);
  } else {
    // Broadcast address, no bits to send
  }
  
  if (p->source_address & 0x00FF0000) {
    buffer[p_buff++] = (p->source_address & 0xFF0000) >> 16;
    buffer[p_buff++] = (p->source_address & 0x00FF00) >> 8;
    buffer[p_buff++] = (p->source_address & 0x0000FF);
  } else if (p->source_address & 0x0000FF00) {
    buffer[p_buff++] = (p->source_address & 0x00FF00) >> 8;
    buffer[p_buff++] = (p->source_address & 0x0000FF);
  } else if (p->source_address & 0x000000FF) {
    buffer[p_buff++] = (p->source_address & 0xFF);
  } else {
    // Broadcast address, no bits to send
  }
  
  // copy the data into the buffer
  for (int rl = p->rlength - 1; rl >= 0; rl--)
    buffer[p_buff + rl] = p->data[rl];
  
  p_buff += p->rlength;
  
  
  // Do the send!
  txBuffer(buffer, p_buff);
}

HARadio::HARadio() {
};
int csn = 10;

void HARadio::spiTable(const prog_uchar *table)
{
    while (1) {
        byte length = pgm_read_byte_near(table++);
        if (!length)
            return;

        digitalWrite(csn, LOW);
        while (length--)
            SPI.transfer(pgm_read_byte_near(table++));
        digitalWrite(csn, HIGH);
	delayMicroseconds(5);
    }
}

void HARadio::DEBUGREG() {
  regRead(0x35);  // MARCSTATE
  regRead(0x38);  // PKTSTATUS
  regRead(0x33);  // LQI
  regRead(0x34);  // RSSI
}

byte HARadio::regRead(byte reg)
{
    digitalWrite(csn, LOW);
    SPI.transfer(0x80 | reg);
    delayMicroseconds(100);
    byte result = SPI.transfer(0);
    delayMicroseconds(100);
    digitalWrite(csn, HIGH);
    return result;
}   

byte HARadio::statusRead()
{
    // Dummy read from register 0
    digitalWrite(csn, LOW);
    byte result = SPI.transfer(0x80);
    digitalWrite(csn, HIGH);
    return result;
} 

void HARadio::reset()
{
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    SPI.setClockDivider(SPI_CLOCK_DIV128);

    // Idle bus state
    pinMode(csn, OUTPUT);
    digitalWrite(csn, HIGH);
    
    // Poll for CHIP_RDYn
    while (statusRead() & 0x80);
    
    delayMicroseconds(10);

    // Table-driven initialization
    const static prog_uchar init[] PROGMEM = {
        1, 0x30,      // SRES        Soft reset strobe
	2, 0x02, 0x06 // IOCFG0 - GDO0Output Pin Configuration 
	2, 0x07, 0x00 // PKTCTRL1 - Packet Automation Control
	2, 0x08, 0x02 // PKTCTRL0 - Packet Automation Control
	2, 0x0A, 0x2A // CHANNR - Channel Number 
	2, 0x0B, 0x0A // FSCTRL1 - Frequency Synthesizer Control 
	2, 0x0D, 0x5d // FREQ2 - Frequency Control Word, High Byte 
	2, 0x0E, 0x44 // FREQ1 - Frequency Control Word, Middle Byte 
	2, 0x10, 0x1B // MDMCFG4 - Modem Configuration 
	2, 0x11, 0x43 // MDMCFG3 - Modem Configuration 
	2, 0x12, 0x7F // MDMCFG2 - Modem Configuration
	2, 0x14, 0x47 // MDMCFG0 - Modem Configuration 
	2, 0x15, 0x01 // DEVIATN - Modem Deviation Setting 
	2, 0x18, 0x18 // MCSM0 - Main Radio Control State Machine Configuration 
	2, 0x19, 0x1D // FOCCFG - Frequency Offset Compensation Configuration
	2, 0x1A, 0x1C // BSCFG - Bit Synchronization Configuration
	2, 0x1B, 0xC7 // AGCCTRL2 - AGC Control
	2, 0x1C, 0x00 // AGCCTRL1 - AGC Control
	2, 0x1D, 0xB0 // AGCCTRL0 - AGC Control
	2, 0x21, 0xB6 // FREND1 - Front End RX Configuration 
	2, 0x25, 0x00 // FSCAL1 - Frequency Synthesizer Calibration 
	2, 0x26, 0x11 // FSCAL0 - Frequency Synthesizer Calibration
	9, 0x7E,      // PATABLE     Set maximum output power
           0xFF, 0xFF, 0xFF, 0xFF,
           0xFF, 0xFF, 0xFF, 0xFF,
        1, 0x3A,      // SFRX        strobe: flush RX fifo
        1, 0x3B,      // SFTX        strobe: flush TX fifo
        1, 0x34,      // SRX         Wait for packets
        0,
    };
    

    spiTable(init);
    
    DEBUGREG();
}

void HARadio::txBuffer(const uint8_t *buffer, uint8_t len) {
    // Prepare for transmit
    const static prog_uchar prepare[] PROGMEM = {
        1, 0x3B,            // SFTX        Flush transmit FIFO
        1, 0x36,            // SIDLE,  Blocks RX, half-duplex only
        0,
    };
    spiTable(prepare);

    // Write packet to TX FIFO
    digitalWrite(csn, LOW);
    SPI.transfer(0x7F);
    SPI.transfer(len+1); // plus 1 for the address field
    SPI.transfer(1); // for the address.  All HA devices are address 0
    while (len--)
        SPI.transfer(*(buffer++));
    digitalWrite(csn, HIGH);

    delayMicroseconds(5);
    DEBUGREG();
    // Trigger the transmit
    const static prog_uchar trigger[] PROGMEM = {
        1, 0x35,            // STX        Enter transmit state
        0,
    };
    spiTable(trigger);
    
    delayMicroseconds(10);
    
    DEBUGREG();
    
    // wait until transmit is done, this is blocking!
    while (1) {
      if ((regRead(0xFA) & 0x7F) == 0)
	break;
      delayMicroseconds(10);
    }
    
    DEBUGREG();
    
    delayMicroseconds(100); // just to be sure?
    
    const static prog_uchar backtoread[] PROGMEM = {
      1, 0x36,
      0,
    };
    spiTable(backtoread);
    delayMicroseconds(1);
    
    DEBUGREG();
}

uint8_t HARadio::rxBuffer(uint8_t *buffer, uint8_t len) {
    uint8_t totalread = 0;
    // Prepare for transmit
    const static prog_uchar prepare[] PROGMEM = {
	1, 0x34,	    // SRX
        0,
    };
    const static prog_uchar prepare2[] PROGMEM = {
	1, 0x36,	    // SIDLE
        0,
    };
    
    DEBUGREG();
    spiTable(prepare);
    DEBUGREG();
    
    delay(10); // wait 10 ms, just to be sure.
    
    uint8_t rxbytes = regRead(0xFB);
    
    // only read the smaller of the two
    totalread = rxbytes > len ? len : rxbytes;
    len = totalread;
    
    if (len > 0) {
      // Write packet to TX FIFO
      digitalWrite(csn, LOW);
      SPI.transfer(0xFF);
      while (len--)
	  *(buffer++) = SPI.transfer(0);
      digitalWrite(csn, HIGH);
    }
    
    delayMicroseconds(5);
    
    DEBUGREG();
    spiTable(prepare2);
    DEBUGREG();
    
    return totalread;
}

