/*!
* @file ToolkitSPI.h
*/

#ifndef ToolkitSPI_H
#define ToolkitSPI_H

#include <Arduino.h>
#include <SPI.h>
/*
typedef enum _BitOrder {
  SPI_BITORDER_MSBFIRST = SPI_MSBFIRST,
  SPI_BITORDER_LSBFIRST = SPI_LSBFIRST,
} BusIOBitOrder;
*/
//
// When we create a new device .. this is what we are doing

// spi_dev_ctrl = new Adafruit_SPIDevice(_cs, 5000000, SPI_BITORDER_MSBFIRST,
//                                         SPI_MODE0, &SPI);

// _cs is unique to each device
// freq is unique
// everything else is constant

class ToolkitSPI
{
    public:
        ToolkitSPI(int8_t cs, uint32_t freq);
        ~ToolkitSPI();
        void begin();            // configure and open the SPI port
        void changeFrequency(uint32_t freq);

        void write(const uint8_t *buffer, size_t len);
        uint16_t write_then_read(const uint8_t *write_buffer, size_t write_len);

    private:
        SPIClass *_spi = NULL;  // used for convenience
        SPISettings *_spiSetting = NULL;    // clock and mode settings
        int8_t _cs; // chip select
};

#endif

//
// END OF ToolkitSPI.h
