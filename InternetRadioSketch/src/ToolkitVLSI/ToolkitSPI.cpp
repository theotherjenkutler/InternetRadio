//
// ToolkitSPI.cpp

#include "ToolkitSPI.h"

ToolkitSPI::ToolkitSPI(int8_t cs, uint32_t freq)
{
    _cs = cs;
    _spi = &SPI;
    _spiSetting = new SPISettings(freq, SPI_MSBFIRST, SPI_MODE0);
}

ToolkitSPI::~ToolkitSPI()
{
    if (_spiSetting) {
      delete _spiSetting;
    }
}

void ToolkitSPI::begin(void)
{
    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
    _spi->begin();
}

void ToolkitSPI::changeFrequency(uint32_t freq)
{
    if (_spiSetting) {
      delete _spiSetting;
    }
    _spiSetting = new SPISettings(freq, SPI_MSBFIRST, SPI_MODE0);
}

void ToolkitSPI::write(const uint8_t *buffer, size_t len)
{
    _spi->beginTransaction(*_spiSetting);
    digitalWrite(_cs, LOW);
    _spi->transferBytes((uint8_t *)buffer, nullptr, len);
    digitalWrite(_cs, HIGH);
    _spi->endTransaction();
}

uint16_t ToolkitSPI::write_then_read(const uint8_t *write_buffer, size_t write_len)
{
    _spi->beginTransaction(*_spiSetting);
    digitalWrite(_cs, LOW);   // chip select

    // do the writing
    _spi->transferBytes((uint8_t *)write_buffer, nullptr, write_len);

    // do the reading
    uint16_t received = _spi->transfer16(0xffff); // VLSI pulls the 0xffff down

    digitalWrite(_cs, HIGH);
    _spi->endTransaction();

    return received;
}

//
// END OF ToolkitSPI.cpp
