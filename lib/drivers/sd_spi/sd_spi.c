/**
 ******************************************************************************
  * @file    sd_spi.c
  * @brief   SD card SPI protocol and block I/O.
  ******************************************************************************
  * Portions copyright (C) 2014, ChaN, all rights reserved.
  * Portions copyright (C) 2017, kiwih, all rights reserved.
  *
  * This software is a free software and there is NO WARRANTY.
  * No restriction on use. You can use, modify and redistribute it for
  * personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
  * Redistributions of source code must retain the above copyright notice.
  *
  ******************************************************************************
  */

/*
 * Maintained adaptation of kiwih/cubeide-sd-card, commit
 * 89261c6a4997232c68ff4f61cc9ecdd20c60e227, based on ChaN's protocol.
 * Hardware access is supplied through sd_spi_port_t; this file has no HAL dependency.
 */
#include "sd_spi.h"
#include <stddef.h>

static sd_spi_port_t port;
static bool ready;
static sd_spi_diagnostics_t diagnostics;
static bool sd_port_block_addressed;
static bool sd_port_io_failed;

static uint8_t sd_port_exchange(uint8_t tx)
{
    uint8_t rx = 0xFF;
    if (!sd_port_io_failed && !port.exchange(tx, &rx)) {
        sd_port_io_failed = true;
        ++diagnostics.transfer_failures;
    }
    return rx;
}

static bool sd_port_set_speed(uint32_t maximum_hz)
{
    if (port.set_speed(maximum_hz)) {
        return true;
    }
    ++diagnostics.speed_failures;
    return false;
}

#define FCLK_SLOW() do { if (!sd_port_set_speed(400000U)) return SD_NOT_READY; } while (0)
#define FCLK_FAST() do { if (!sd_port_set_speed(4000000U)) return SD_NOT_READY; } while (0)
#define CS_HIGH() port.chip_select(false)
#define CS_LOW() port.chip_select(true)

/* MMC/SD command */
#define CMD0	(0)			/* GO_IDLE_STATE */
#define CMD1	(1)			/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)			/* SEND_IF_COND */
#define CMD9	(9)			/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	(32)		/* ERASE_ER_BLK_START */
#define CMD33	(33)		/* ERASE_ER_BLK_END */
#define CMD38	(38)		/* ERASE */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */

/* MMC card type flags (MMC_GET_TYPE) */
#define CT_MMC		0x01		/* MMC ver 3 */
#define CT_SD1		0x02		/* SD ver 1 */
#define CT_SD2		0x04		/* SD ver 2 */
#define CT_SDC		(CT_SD1|CT_SD2)	/* SD */
#define CT_BLOCK	0x08		/* Block addressing */


static
uint8_t CardType;			/* Card type flags */

static uint32_t spiTimerTickStart;
static uint32_t spiTimerTickDelay;

static void SPI_Timer_On(uint32_t waitTicks) {
    spiTimerTickStart = port.get_tick_ms();
    spiTimerTickDelay = waitTicks;
}

static bool SPI_Timer_Status(void) {
    return ((port.get_tick_ms() - spiTimerTickStart) < spiTimerTickDelay);
}

/*-----------------------------------------------------------------------*/
/* SPI controls (Platform dependent)                                     */
/*-----------------------------------------------------------------------*/

/* Exchange a byte */
static
uint8_t xchg_spi (
	uint8_t dat	/* Data to send */
)
{
	return sd_port_exchange(dat);
}


/* Receive multiple byte */
static
void rcvr_spi_multi (
	uint8_t *buff,		/* Pointer to data buffer */
	uint32_t btr		/* Number of bytes to receive (even number) */
)
{
	for(uint32_t i=0; i<btr && !sd_port_io_failed; i++) {
		*(buff+i) = xchg_spi(0xFF);
	}
}


/* Send multiple byte */
static
void xmit_spi_multi (
	const uint8_t *buff,	/* Pointer to the data */
	uint32_t btx			/* Number of bytes to send (even number) */
)
{
	for (uint32_t i = 0; i < btx && !sd_port_io_failed; ++i) xchg_spi(buff[i]);
}


/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/

static
int wait_ready (	/* 1:Ready, 0:Timeout */
	uint32_t wt			/* Timeout [ms] */
)
{
	uint8_t d;
	//wait_ready needs its own timer, unfortunately, so it can't use the
	//spi_timer functions
	uint32_t waitSpiTimerTickStart;
	uint32_t waitSpiTimerTickDelay;

	waitSpiTimerTickStart = port.get_tick_ms();
	waitSpiTimerTickDelay = (uint32_t)wt;
	do {
		d = xchg_spi(0xFF);
		/* This loop takes a time. Insert rot_rdq() here for multitask envilonment. */
	} while (d != 0xFF && !sd_port_io_failed && ((port.get_tick_ms() - waitSpiTimerTickStart) < waitSpiTimerTickDelay));	/* Wait for card goes ready or timeout */

	return (d == 0xFF && !sd_port_io_failed) ? 1 : 0;
}


/*-----------------------------------------------------------------------*/
/* Deselect card and release SPI                                         */
/*-----------------------------------------------------------------------*/

static
void deselect_card (void)
{
	CS_HIGH();		/* Set CS# high */
	xchg_spi(0xFF);	/* Dummy clock (force DO hi-z for multiple slave SPI) */

}


/*-----------------------------------------------------------------------*/
/* Select card and wait for ready                                        */
/*-----------------------------------------------------------------------*/

static
int spiselect (void)	/* 1:OK, 0:Timeout */
{
	CS_LOW();		/* Set CS# low */
	xchg_spi(0xFF);	/* Dummy clock (force DO enabled) */
	if (wait_ready(500)) return 1;	/* Wait for card ready */

	deselect_card();
	return 0;	/* Timeout */
}


/*-----------------------------------------------------------------------*/
/* Receive a data packet from the MMC                                    */
/*-----------------------------------------------------------------------*/

static
int rcvr_datablock (	/* 1:OK, 0:Error */
	uint8_t *buff,			/* Data buffer */
	uint32_t btr			/* Data block length (byte) */
)
{
	uint8_t token;


	SPI_Timer_On(200);
	do {							/* Wait for DataStart token in timeout of 200ms */
		token = xchg_spi(0xFF);
		/* This loop will take a time. Insert rot_rdq() here for multitask envilonment. */
	} while ((token == 0xFF) && !sd_port_io_failed && SPI_Timer_Status());
	if(token != 0xFE) return 0;		/* Function fails if invalid DataStart token or timeout */

	rcvr_spi_multi(buff, btr);		/* Store trailing data to the buffer */
	xchg_spi(0xFF); xchg_spi(0xFF);			/* Discard CRC */

	return !sd_port_io_failed; /* Propagate byte transfer failures. */
}


/*-----------------------------------------------------------------------*/
/* Send a data packet to the MMC                                         */
/*-----------------------------------------------------------------------*/

static
int xmit_datablock (	/* 1:OK, 0:Failed */
	const uint8_t *buff,	/* Ponter to 512 byte data to be sent */
	uint8_t token			/* Token */
)
{
	uint8_t resp;


	if (!wait_ready(500)) return 0;		/* Wait for card ready */

	xchg_spi(token);					/* Send token */
	if (token != 0xFD) {				/* Send data if token is other than StopTran */
		xmit_spi_multi(buff, 512);		/* Data */
		xchg_spi(0xFF); xchg_spi(0xFF);	/* Dummy CRC */

		resp = xchg_spi(0xFF);				/* Receive data resp */
		if ((resp & 0x1F) != 0x05 || sd_port_io_failed) {
			(void)wait_ready(500);
			return 0;
		}
	}
	return wait_ready(500) && !sd_port_io_failed;
}


/*-----------------------------------------------------------------------*/
/* Send a command packet to the MMC                                      */
/*-----------------------------------------------------------------------*/

static
uint8_t send_cmd (		/* Return value: R1 resp (bit7==1:Failed to send) */
	uint8_t cmd,		/* Command index */
	uint32_t arg		/* Argument */
)
{
	uint8_t n, res;


	if (cmd & 0x80) {	/* Send a CMD55 prior to ACMD<n> */
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1) return res;
	}

	/* Select the card and wait for ready except to stop multiple block read */
	if (cmd != CMD12) {
		deselect_card();
		if (!spiselect()) return 0xFF;
	}

	/* Send command packet */
	xchg_spi(0x40 | cmd);				/* Start + command index */
	xchg_spi((uint8_t)(arg >> 24));		/* Argument[31..24] */
	xchg_spi((uint8_t)(arg >> 16));		/* Argument[23..16] */
	xchg_spi((uint8_t)(arg >> 8));			/* Argument[15..8] */
	xchg_spi((uint8_t)arg);				/* Argument[7..0] */
	n = 0x01;							/* Dummy CRC + Stop */
	if (cmd == CMD0) n = 0x95;			/* Valid CRC for CMD0(0) */
	if (cmd == CMD8) n = 0x87;			/* Valid CRC for CMD8(0x1AA) */
	xchg_spi(n);

	/* Receive command resp */
	if (cmd == CMD12) xchg_spi(0xFF);	/* Discard the CMD12 stuff byte. */
	n = 10;								/* Wait for response (10 bytes max) */
	do {
		res = xchg_spi(0xFF);
	} while ((res & 0x80) && --n && !sd_port_io_failed);

	return res;							/* Return received response */
}


/*--------------------------------------------------------------------------

   Private SD protocol functions

---------------------------------------------------------------------------*/

/* Private protocol; sd_spi.c validates state and arguments. */


/*-----------------------------------------------------------------------*/
/* Initialize disk drive                                                 */
/*-----------------------------------------------------------------------*/

static sd_result_t sd_protocol_initialize(void)
{
	uint8_t n, cmd, ty, ocr[4];
	CardType = 0;
	CS_HIGH();
	port.delay_ms(10U);

	//assume SPI already init init_spi();	/* Initialize SPI */


	FCLK_SLOW();
	for (n = 10; n; n--) xchg_spi(0xFF);	/* Send 80 dummy clocks */

	ty = 0;
	if (send_cmd(CMD0, 0) == 1) {			/* Put the card SPI/Idle state */
		SPI_Timer_On(1000);					/* Initialization timeout = 1 sec */
		uint8_t response = send_cmd(CMD8, 0x1AA);
		if (response == 1) {	/* SDv2 */
			for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);	/* Get 32 bit return value of R7 resp */
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) {				/* Is the card supports vcc of 2.7-3.6V? */
				while (SPI_Timer_Status() && !sd_port_io_failed && send_cmd(ACMD41, 1UL << 30)) ;	/* Wait for end of initialization with ACMD41(HCS) */
				if (SPI_Timer_Status() && !sd_port_io_failed && send_cmd(CMD58, 0) == 0) {		/* Check CCS bit in the OCR */
					for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);
					/* OCR must confirm that card initialization has completed. */
					if (ocr[0] & 0x80) {
						ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;
					}
				}
			}
		} else if (response == 5) {	/* Idle + illegal command: legacy card. */
			if (send_cmd(ACMD41, 0) <= 1) 	{	/* SDv1 or MMC? */
				ty = CT_SD1; cmd = ACMD41;	/* SDv1 (ACMD41(0)) */
			} else {
				ty = CT_MMC; cmd = CMD1;	/* MMCv3 (CMD1(0)) */
			}
			while (SPI_Timer_Status() && !sd_port_io_failed && send_cmd(cmd, 0)) ;		/* Wait for end of initialization */
			if (!SPI_Timer_Status() || send_cmd(CMD16, 512) != 0)	/* Set block length: 512 */
				ty = 0;
		}
	}
	/* SDv2 SDSC also needs 512-byte block length. */
	if (ty == CT_SD2 && send_cmd(CMD16, 512) != 0) ty = 0;
	if (sd_port_io_failed) ty = 0;
	CardType = ty;
	sd_port_block_addressed = (ty & CT_BLOCK) != 0;
	deselect_card();

	if (!ty) return SD_NOT_READY;
	FCLK_FAST();
	return SD_OK;
}


/*-----------------------------------------------------------------------*/
/* Read sector(s)                                                        */
/*-----------------------------------------------------------------------*/

static sd_result_t sd_protocol_read(uint32_t sector, uint8_t *buff, uint32_t count)
{

	if (!(CardType & CT_BLOCK)) sector *= 512;	/* Convert LBA to byte addressing for SDSC. */

	if (count == 1) {	/* Single sector read */
		if ((send_cmd(CMD17, sector) == 0)	/* READ_SINGLE_BLOCK */
			&& rcvr_datablock(buff, 512)) {
			count = 0;
		}
	}
	else {				/* Multiple sector read */
		if (send_cmd(CMD18, sector) == 0) {	/* READ_MULTIPLE_BLOCK */
			do {
				if (!rcvr_datablock(buff, 512)) break;
				buff += 512;
			} while (--count);
			uint8_t stopped = send_cmd(CMD12, 0);
			if (!wait_ready(500) || stopped != 0) count = 1;
		}
	}
	deselect_card();

	return count || sd_port_io_failed ? SD_ERROR : SD_OK;	/* Return result */
}


/*-----------------------------------------------------------------------*/
/* Write sector(s)                                                       */
/*-----------------------------------------------------------------------*/

static sd_result_t sd_protocol_write(uint32_t sector, const uint8_t *buff, uint32_t count)
{

	if (!(CardType & CT_BLOCK)) sector *= 512;	/* LBA ==> BA conversion (byte addressing cards) */

	if (count == 1) {	/* Single sector write */
		if ((send_cmd(CMD24, sector) == 0)	/* WRITE_BLOCK */
			&& xmit_datablock(buff, 0xFE)) {
			count = 0;
		}
	}
	else {				/* Multiple sector write */
		if (CardType & CT_SDC) send_cmd(ACMD23, count);	/* Predefine number of sectors */
		if (send_cmd(CMD25, sector) == 0) {	/* WRITE_MULTIPLE_BLOCK */
			do {
				if (!xmit_datablock(buff, 0xFC)) break;
				buff += 512;
			} while (--count);
			if (!xmit_datablock(0, 0xFD)) count = 1;	/* STOP_TRAN token */
		}
	}
	/* Wait for completion and check R2 write status before reporting success. */
	if (!count) {
		uint8_t r1 = send_cmd(13, 0);
		uint8_t r2 = xchg_spi(0xFF);
		if (r1 != 0 || r2 != 0) count = 1;
	}
	deselect_card();

	return count || sd_port_io_failed ? SD_ERROR : SD_OK;
}


/*-----------------------------------------------------------------------*/
/* Miscellaneous drive controls other than data read/write               */
/*-----------------------------------------------------------------------*/

static sd_result_t sd_protocol_sync(void)
{
    bool card_ready = spiselect() != 0;
    deselect_card();
    return card_ready && !sd_port_io_failed ? SD_OK : SD_ERROR;
}

static sd_result_t sd_protocol_get_sector_count(uint32_t *count)
{
    uint8_t csd[16];
    sd_result_t result = SD_ERROR;
    if (send_cmd(CMD9, 0) == 0 && rcvr_datablock(csd, sizeof(csd))) {
        uint32_t csize;
        if ((csd[0] >> 6) == 1) {
            csize = csd[9] + ((uint32_t)csd[8] << 8) +
                ((uint32_t)(csd[7] & 63) << 16) + 1U;
            if (csize <= (UINT32_MAX >> 10)) {
                *count = csize << 10;
                result = SD_OK;
            }
        } else if ((csd[0] >> 6) == 0) {
            uint32_t n = (csd[5] & 15) + ((csd[10] & 128) >> 7) +
                ((csd[9] & 3) << 1) + 2U;
            csize = (csd[8] >> 6) + ((uint32_t)csd[7] << 2) +
                ((uint32_t)(csd[6] & 3) << 10) + 1U;
            if (n >= 9U && n - 9U < 32U && csize <= (UINT32_MAX >> (n - 9U))) {
                *count = csize << (n - 9U);
                result = SD_OK;
            }
        }
    }
    deselect_card();
    return sd_port_io_failed ? SD_ERROR : result;
}

bool sd_spi_bind(const sd_spi_port_t *hardware)
{
    ready = false;
    port = (sd_spi_port_t){0};
    if (hardware == NULL || hardware->exchange == NULL || hardware->set_speed == NULL ||
        hardware->chip_select == NULL || hardware->get_tick_ms == NULL || hardware->delay_ms == NULL) {
        ++diagnostics.invalid_arguments;
        diagnostics.last_result = SD_INVALID_ARGUMENT;
        return false;
    }
    port = *hardware;
    port.chip_select(false);
    return true;
}

sd_result_t sd_spi_initialize(void)
{
    ready = false;
    sd_port_io_failed = false;
    sd_port_block_addressed = false;
    sd_result_t result = port.exchange == NULL ? SD_NOT_READY : sd_protocol_initialize();
    if (sd_port_io_failed) {
        result = SD_ERROR;
    }
    ready = result == SD_OK;
    if (!ready) {
        ++diagnostics.initialization_failures;
    }
    diagnostics.last_result = result;
    return result;
}

bool sd_spi_is_ready(void)
{
    return ready;
}

static bool valid_sector_range(uint32_t sector, uint32_t count)
{
    uint32_t last_sector = sd_port_block_addressed ? UINT32_MAX : UINT32_MAX / SD_SPI_SECTOR_SIZE;
    return count != 0U && count <= UINT32_MAX / SD_SPI_SECTOR_SIZE &&
        sector <= last_sector && count - 1U <= last_sector - sector;
}

static sd_result_t record_rejection(sd_result_t result)
{
    if (result == SD_INVALID_ARGUMENT) {
        ++diagnostics.invalid_arguments;
    } else if (result == SD_NOT_READY) {
        ++diagnostics.not_ready_requests;
    }
    diagnostics.last_result = result;
    return result;
}

static sd_result_t finish_operation(sd_result_t result, uint32_t *failures)
{
    if (sd_port_io_failed) {
        result = SD_ERROR;
    }
    if (result != SD_OK) {
        ready = false;
        ++*failures;
    }
    diagnostics.last_result = result;
    return result;
}

sd_result_t sd_spi_read(uint32_t sector, uint8_t *buffer, uint32_t count)
{
    if (buffer == NULL || !valid_sector_range(sector, count)) {
        return record_rejection(SD_INVALID_ARGUMENT);
    }
    if (!ready) {
        return record_rejection(SD_NOT_READY);
    }
    sd_port_io_failed = false;
    return finish_operation(sd_protocol_read(sector, buffer, count), &diagnostics.read_failures);
}

sd_result_t sd_spi_write(uint32_t sector, const uint8_t *buffer, uint32_t count)
{
    if (buffer == NULL || !valid_sector_range(sector, count)) {
        return record_rejection(SD_INVALID_ARGUMENT);
    }
    if (!ready) {
        return record_rejection(SD_NOT_READY);
    }
    sd_port_io_failed = false;
    return finish_operation(sd_protocol_write(sector, buffer, count), &diagnostics.write_failures);
}

sd_result_t sd_spi_sync(void)
{
    if (!ready) {
        return record_rejection(SD_NOT_READY);
    }
    sd_port_io_failed = false;
    return finish_operation(sd_protocol_sync(), &diagnostics.sync_failures);
}

sd_result_t sd_spi_get_sector_count(uint32_t *count)
{
    if (count == NULL) {
        return record_rejection(SD_INVALID_ARGUMENT);
    }
    if (!ready) {
        return record_rejection(SD_NOT_READY);
    }
    sd_port_io_failed = false;
    return finish_operation(sd_protocol_get_sector_count(count), &diagnostics.capacity_failures);
}

void sd_spi_get_diagnostics(sd_spi_diagnostics_t *out)
{
    if (out != NULL) {
        *out = diagnostics;
    }
}
