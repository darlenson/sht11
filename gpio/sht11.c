/* This file is part of sht11
 * Copyright (C) 2005-2010 Enrico Rossi
 * 
 * Sht11 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Sht11 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include "sht11_io.h"
#include "sht11.h"

extern struct sht11_t *sht11;

void send_byte(uint8_t byte)
{
	uint8_t i;

	i = 8;

	while (i) {
		--i;

		if (byte & (1<<i))
			set_data_high();
		else
			set_data_low();

		sck_delay();

		set_sck_high();
		sck_delay();

		set_sck_low();
	}
}

uint8_t read_byte(void)
{
	uint8_t i, bit, result;

	result = 0;
	i = 8;

	while (i) {
		--i;
		sck_delay();
		set_sck_high();

		bit = read_data_pin();

		sck_delay();
		set_sck_low();

		if (bit)
			result |= (1<<i);
	}

	return (result);
}

void send_ack(void)
{
	/* Send ack */
	set_data_low();
	sck_delay();
	set_sck_high();
	sck_delay();
	set_sck_low();
	set_data_in();
}

uint8_t read_ack(void)
{
	uint8_t ack;

	/* read ack after command */
	set_data_in();
	sck_delay();
	set_sck_high();

	ack = read_data_pin();

	sck_delay();
	set_sck_low();

	return (ack);
}

void send_start_command(void)
{
	/* DATA:   _____           ________
	   DATA:         |_______|
	   SCK :       ___       ___
	   SCK :  ___|     |___|     |______
	 */

	set_data_high();
	/*   set_data_out (); */
	sck_delay();
	set_sck_high();
	sck_delay();
	set_data_low();
	sck_delay();
	set_sck_low();
	sck_delay();
	set_sck_high();
	sck_delay();
	set_data_high();
	sck_delay();
	set_sck_low();
	sck_delay();
}

void sht11_init(void)
{
	sht11_io_init();
}

void sht11_end(void)
{
	sht11_io_end();
}

uint8_t sht11_read_status_reg(void)
{
	uint8_t result, crc8, ack;

	result = 0;
	crc8 = 0;
	send_start_command();

	/* send read status register command */
	send_byte(7);
	ack = read_ack();

	if (ack)
		return (0);

	result = read_byte();

	/* Send ack */
	send_ack();

	/* inizio la lettura del CRC-8 */
	crc8 = read_byte();

	send_ack();
	return (result);
}

/* Disable Interrupt to avoid possible clk problem. */
void send_command(uint8_t command, uint16_t *result, uint8_t *crc8)
{
	uint8_t ack;

	/* safety 000xxxxx */
	command &= 31;
	*result = 0;
	*crc8 = 0;

	send_start_command();
	send_byte(command);
	ack = read_ack();

	if (!ack) {
		/* And if nothing came back this code hangs here */
		wait_until_data_is_ready();

		/* inizio la lettura dal MSB del primo byte */
		*result = read_byte() << 8;

		/* Send ack */
		send_ack();

		/* inizio la lettura dal MSB del secondo byte */
		*result |= read_byte();

		send_ack();

		/* inizio la lettura del CRC-8 */
		*crc8 = read_byte();

		/* do not Send ack */
		set_data_high();
		sck_delay();
		set_sck_high();
		sck_delay();
		set_sck_low();
		set_data_in();
	}
}

void dewpoint(void)
{
	double k;

	k = (log10(sht11->humidity_compensated) - 2) / 0.4343 + (17.62 * sht11->temperature) / (243.12 + sht11->temperature);
	sht11->dewpoint = 243.12 * k / (17.62 - k);
}

void sht11_read_temperature(void)
{
	send_command(SHT11_CMD_MEASURE_TEMP, &sht11->raw_temperature, &sht11->raw_temperature_crc8);
	sht11->temperature = SHT11_T1 * sht11->raw_temperature - 40;
}

void sht11_read_humidity(void)
{
	send_command(SHT11_CMD_MEASURE_HUMI, &sht11->raw_humidity, &sht11->raw_humidity_crc8);

	sht11->humidity_linear = SHT11_C1 + (SHT11_C2 * sht11->raw_humidity) + (SHT11_C3 * sht11->raw_humidity * sht11->raw_humidity);

	/* Compensate humidity result with temperature */
	sht11->humidity_compensated = (sht11->temperature - 25) * (SHT11_T1 + SHT11_T2 * sht11->raw_humidity) + sht11->humidity_linear;

	/* Range adjustment */
	if (sht11->humidity_compensated > 100)
		sht11->humidity_compensated = 100;
	if (sht11->humidity_compensated < 0.1)
		sht11->humidity_compensated = 0.1;
}

void sht11_read_all(void)
{
	sht11_read_temperature();
	sht11_read_humidity();
	dewpoint();
}
