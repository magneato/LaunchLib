/**
@file uart.c
@brief Blocking and non-blocking UART interface
@author Joe Brown
*/
#include "global.h"
#include "uart.h"
#include "hardware.h"

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//                            ____        _  __
//                           /  _/____   (_)/ /_
//                           / / / __ \ / // __/
//                         _/ / / / / // // /_
//                        /___//_/ /_//_/ \__/
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/** @brief baud rate lookup table */
static const BaudRateConfig rate_table[] =
{
#ifdef __MSP430G2553__
    // see userguide table 15-5
    {  9600,  1000000,   6,    0,  8},
    { 19200,  1000000,   3,    0,  4},
    { 57600,  1000000,   1,    7,  0},
    {  9600,  8000000,  52,    0,  1},
    { 19200,  8000000,  26,    0,  1},
    { 38400,  8000000,  13,    0,  0},
    { 57600,  8000000,   8,    0, 11},
    {115200,  8000000,   4,    5,  3},
    {  9600, 12000000,  78,    0,  2},
    { 19200, 12000000,  39,    0,  1},
    { 38400, 12000000,  19,    0,  8},
    { 57600, 12000000,  13,    0,  0},
    {115200, 12000000,   6,    0,  8},
    {  9600, 16000000, 104,    0,  3},
    { 19200, 16000000,  52,    0,  1},
    { 38400, 16000000,  26,    0,  1},
    { 57600, 16000000,  17,    0,  6},
    {115200, 16000000,   8,    0, 11}
#elif __MSP430FR5739__
    // see userguide table 18-5
    {  9600,  8000000,  52, 0x49,  1},
    { 19200,  8000000,  26, 0xB6,  0},
    { 38400,  8000000,  13, 0x84,  0},
    { 57600,  8000000,   8, 0xF7, 10},
    {115200,  8000000,   4, 0x55,  5},
    {  9600, 16000000, 104, 0xD6,  2},
    { 19200, 16000000,  52, 0x49,  1},
    { 38400, 16000000,  26, 0xB6,  0},
    { 57600, 16000000,  17, 0xDD,  5},
    {115200, 16000000,   8, 0xF7, 10},
    {  9600, 20000000, 104, 0x25,  3},
    { 19200, 20000000,  52, 0xD6,  1},
    { 38400, 20000000,  26, 0xEE,  8},
    { 57600, 20000000,  17, 0x22, 11},
    {115200, 20000000,   8, 0xAD, 13},
    {  9600, 24000000, 156, 0x00,  4},
    { 19200, 24000000,  78, 0x00,  2},
    { 38400, 24000000,  39, 0x00,  1},
    { 57600, 24000000,  26, 0xD6,  0},
    {115200, 24000000,  13, 0x49,  0}
#endif
};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void UartInit(uint32_t baud_rate)
{
    uint8_t i = 0;
    uint8_t index = 0;
    // find where the baud rate is in the table
    for(i = 0;i < sizeof(rate_table) / sizeof(BaudRateConfig);i++)
    {
        if (baud_rate == rate_table[i].baud &&
            g_clock_speed == rate_table[i].clock)
        {
            index = i;
            break;
        }
    }

    // SMCLK sources uart
    UCA0CTL1 |= UCSSEL_2;
    // set the divisor
    UCA0BR0 = rate_table[index].ucbr;
    // we always use modulation and never go below 9600 baud which means BR will
    // always be < 256
    UCA0BR1 = 0;
    // UCBRS is at bits 1-3
    UCA0MCTL = rate_table[index].ucbrs << UCBRS_OFFSET;
    // UCBRF is at bits 4-7
    UCA0MCTL |= rate_table[index].ucbrf << USBRF_OFFSET;
    // set UCOS16 to enable oversampling
    UCA0MCTL |= UCOS16;

    // Initialize USCI state machine
    UCA0CTL1 &= ~UCSWRST;
    // Enable USCI_A0 RX and TX interrupts
#ifdef NON_BLOCKING_UART_TX
    UART_INT_ENABLE |= UCA0TXIE;
#endif
#ifdef NON_BLOCKING_UART_RX
    UART_INT_ENABLE |= UCA0RXIE;
#endif
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//                ____                     _
//               / __ \ ___   _____ ___   (_)_   __ ___
//              / /_/ // _ \ / ___// _ \ / /| | / // _ \
//             / _, _//  __// /__ /  __// / | |/ //  __/
//            /_/ |_| \___/ \___/ \___//_/  |___/ \___/
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
#ifdef NON_BLOCKING_UART_RX
/** @brief start index for rx circular buffer */
static uint8_t rx_start = 0;
/** @brief size of rx circular buffer */
static uint8_t rx_size  = 0;
/** @brief rx buffer */
static uint8_t rx_buf[MAX_UART_RX_BUF_CNT];

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
int8_t RxBufferEnqueue(uint8_t data)
{
    // is the buffer full?
    if (rx_size == MAX_UART_RX_BUF_CNT)
    {
        goto error;
    }

    // stuff the data
    if((rx_start + rx_size) == MAX_UART_RX_BUF_CNT)
    {
        rx_buf[0] = data;
    }
    else
    {
        rx_buf[(rx_start + rx_size)] = data;
    }

    // adjust the buf size
    rx_size++;

    return (0);
error:
    return (-1);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
uint8_t RxBufferDequeue(uint8_t *in, uint16_t len)
{
    uint8_t len_to_read;
    _DINT();
    // is the buffer empty?
    if (rx_size == 0)
    {
        goto error;
    }

    // are we trying to read more than is in the buffer?
    len_to_read = (len > rx_size) ? rx_size : len;

    // copy the data from the rx buffer into the input buffer
    // does the data cross the end of the buffer?
    if ((rx_start + rx_size) <= rx_start)
    {
        // break up the copy into two parts
        // from start index to the end of the buffer
        memmove(in,
               (rx_buf + rx_start),
               (MAX_UART_RX_BUF_CNT - rx_start));
        // from the beginning of the buffer to the end index
        memmove(in + (MAX_UART_RX_BUF_CNT - rx_start),
               rx_buf,
               (len_to_read - (MAX_UART_RX_BUF_CNT - rx_start)));
    }
    else
    {
        memmove(in, rx_buf + rx_start, len_to_read);
    }

    // adjust the start index
    if((rx_start + rx_size) >= MAX_UART_RX_BUF_CNT)
    {
        rx_start = ((rx_start + len_to_read) - MAX_UART_RX_BUF_CNT);
    }
    else
    {
        rx_start = (rx_start + len_to_read);
    }

    // adjust the size
    rx_size -= len_to_read;

    _EINT();
    return len_to_read;
error:
    _EINT();
    return (0);
}
#endif // NON_BLOCKING_UART_TX

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//             ______                                      _  __
//            /_  __/_____ ____ _ ____   _____ ____ ___   (_)/ /_
//             / /  / ___// __ `// __ \ / ___// __ `__ \ / // __/
//            / /  / /   / /_/ // / / /(__  )/ / / / / // // /_
//           /_/  /_/    \__,_//_/ /_//____//_/ /_/ /_//_/ \__/
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
#ifdef NON_BLOCKING_UART_TX
/** @brief start index for tx circular buffer */
static uint8_t tx_start = 0;
/** @brief size of tx circular buffer */
static uint8_t tx_size  = 0;
/** @brief tx buffer */
static uint8_t tx_buf[MAX_UART_TX_BUF_CNT];

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
int8_t TxBufferEnqueue(uint8_t data)
{
    _DINT();
    // is the buffer full?
    if (tx_size == MAX_UART_TX_BUF_CNT)
    {
        goto error;
    }

    // stuff the data
    // although we only dequeue and queue single bytes at a time (because that's
    // all the hardware supports) we still need to deal with multi-byte boundary
    // crossing as we may be filling the queue faster than the tx interrupt can
    // pull it out.
    if ((tx_start + tx_size) >= MAX_UART_TX_BUF_CNT)
    {
        tx_buf[(tx_start + tx_size) - MAX_UART_TX_BUF_CNT] = data;
    }
    else
    {
        tx_buf[(tx_start + tx_size)] = data;
    }

    // adjust the buf size
    tx_size++;

    _EINT();
    return (0);
error:
    _EINT();
    return (-1);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
uint8_t TxBufferDequeue(void)
{
    uint8_t ret = 0;
    // is the buffer empty?
    if (tx_size != 0)
    {
        // return data
        ret = tx_buf[tx_start];
        // adjust the start
        tx_start = (tx_start == (MAX_UART_TX_BUF_CNT - 1)) ? 0 : tx_start + 1;
        // adjust the size
        tx_size--;
    }
    return ret;
}
#endif //NON_BLOCKING_UART_TX

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//        ____        __                                   __
//       /  _/____   / /_ ___   _____ _____ __  __ ____   / /_ _____
//       / / / __ \ / __// _ \ / ___// ___// / / // __ \ / __// ___/
//     _/ / / / / // /_ /  __// /   / /   / /_/ // /_/ // /_ (__  )
//    /___//_/ /_/ \__/ \___//_/   /_/    \__,_// .___/ \__//____/
//                                             /_/
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
#ifdef __MSP430G2553__
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
#ifdef NON_BLOCKING_UART_RX
#pragma vector=UART_RX_INT_VECTOR
__interrupt void UartRxInt(void)
{
    RxBufferEnqueue(UCA0RXBUF);
}
#endif //NON_BLOCKING_UART_RX

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
#ifdef NON_BLOCKING_UART_TX
#pragma vector = UART_TX_INT_VECTOR
__interrupt void UartTxInt(void)
{
    UCA0TXBUF = TxBufferDequeue();
}
#endif //NON_BLOCKING_UART_TX
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
#elif __MSP430FR5739__
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
#if defined NON_BLOCKING_UART_RX || defined NON_BLOCKING_UART_TX
#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
{
    switch(__even_in_range(UCA0IV,0x08))
    {
        case 0: // no interrupt
            break;
        case 2: // RXIFG
#ifdef NON_BLOCKING_UART_RX
            RxBufferEnqueue(UCA0RXBUF);
#endif
            break;
        case 4: //TXIFG
#ifdef NON_BLOCKING_UART_TX
            UCA0TXBUF = TxBufferDequeue();
#endif
            break;
        default:
            break;
    }
}
#endif

#endif
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//                    ____         _         __   ____
//                   / __ \ _____ (_)____   / /_ / __/
//                  / /_/ // ___// // __ \ / __// /_
//                 / ____// /   / // / / // /_ / __/
//                /_/    /_/   /_//_/ /_/ \__//_/
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
// Code adapted from 43oh.com forums
/** @brief decimal value lookup table */
static const uint32_t decimal_table[] =
{
    1000000000, // +0
     100000000, // +1
      10000000, // +2
       1000000, // +3
        100000, // +4
         10000, // +5
          1000, // +6
           100, // +7
            10, // +8
             1, // +9
             0
};

#define hex(m) char_table[m & 0x0F]

/** @brief character lookup table */
static const uint8_t char_table[] = "0123456789ABCDEF";

/** @brief hex value lookup table */
static const uint32_t hex_table[] =
{
    0x10000000,
    0x01000000,
    0x00100000,
    0x00010000,
    0x00001000,
    0x00000100,
    0x00000010,
    0x00000001,
    0x00000000
};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void UartPutIToA(uint32_t value, enum NUMBER_BASE base)
{
    uint32_t position = 0;
    uint8_t index = 0;

    // pointer to which table we're using
    const uint32_t* selected_table;
    selected_table = (base == HEX) ? hex_table : decimal_table;

    // work our way down to the current decade within that table
    while (value < *selected_table)
    {
        selected_table++;
    }
    do
    {
        index = 0;
        position = *selected_table++;
        while((value >= position) && (position != 0))
        {
            index++;            // increment into the char table
            value -= position;  // decrement to the next value
        }
        UartPutC(hex(index));
    } while(position > 1);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//            ____        __               ____
//           /  _/____   / /_ ___   _____ / __/____ _ _____ ___
//           / / / __ \ / __// _ \ / ___// /_ / __ `// ___// _ \
//         _/ / / / / // /_ /  __// /   / __// /_/ // /__ /  __/
//        /___//_/ /_/ \__/ \___//_/   /_/   \__,_/ \___/ \___/
//
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void UartPrintf(uint8_t *format, ...)
{
    uint8_t current_char;
    int16_t current_short_int;
    int32_t current_long_int;

    va_list arg_list;
    va_start(arg_list, format);
    while(current_char = *format++)
    {
        if(current_char == '%')
        {
            switch(current_char = *format++)
            {
                case 's':   // String
                    UartPutS(va_arg(arg_list, uint8_t*));
                    break;
                case 'c':   // Char
                    UartPutC(va_arg(arg_list, uint8_t));
                    break;
                case 'l':   // 32 bit
                    switch(current_char = *format++)
                    {
                        case 'd':   // 32 bit signed
                        case 'i':   // 32 bit signed
                        case 'u':   // 32 bit unsigned
                            current_long_int = va_arg(arg_list, uint32_t);
                            // if we're negative
                            if(current_char != 'u' && current_long_int < 0)
                            {
                                // add the negative sign
                                UartPutC('-');
                                // swap the value to positive to print
                                current_long_int = -current_long_int;
                            }
                            UartPutIToA(current_long_int, DECIMAL);
                            break;
                        default:
                            goto bad_fmt;
                    }
                    break;
                case 'd':   // 16 bit signed
                case 'i':   // 16 bit signed
                case 'u':   // 16 bit unsigned
                    current_short_int = va_arg(arg_list, uint16_t);
                    // if we're negative
                    if(current_char != 'u' && current_short_int < 0)
                    {
                        // add the negative sign
                        UartPutC('-');
                        // swap the value to positive to print
                        current_short_int = -current_short_int;
                    }
                    UartPutIToA(current_short_int, DECIMAL);
                    break;
                case 'x':   // 32 bit hex
                    current_long_int = va_arg(arg_list, uint32_t);
                    UartPutIToA(current_long_int, HEX);
                    break;
                case 0:
                    return;
                default:
                    goto bad_fmt;
            }
        }
        else
        {
bad_fmt:
            UartPutC(current_char);
        }
    }
    va_end(arg_list);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
uint8_t UartRead(uint8_t *in, uint16_t len)
{
#ifdef NON_BLOCKING_UART_RX
    return RxBufferDequeue(in, len);
#else
    in[0] = UartGetC();
    return (1);
#endif
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void UartWrite(uint8_t *in, uint16_t len)
{
    uint8_t i = 0;
    for(i = 0;i < len;i++)
    {
#ifdef NON_BLOCKING_UART_TX
        TxBufferEnqueue(in[i]);
#else
        UartPutC(in[i]);
#endif
    }
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
uint8_t UartGetC(void)
{
#ifdef NON_BLOCKING_UART_RX
    uint8_t buf = 0;
    RxBufferDequeue(&buf,1);
    return buf;
#else
    while (!(UART_INT_FLAG & UCA0RXIFG));    // RX a byte?
    return UCA0RXBUF;
#endif

}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void UartPutC(uint8_t data)
{
#ifdef NON_BLOCKING_UART_TX
    TxBufferEnqueue(data);
#else
    while (!(UART_INT_FLAG & UCA0TXIFG));    // TX buffer ready?
    UCA0TXBUF = data;
#endif
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
void UartPutS(uint8_t *s)
{
    while(*s)
    {
        UartPutC(*s++);
    }
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
#ifdef NON_BLOCKING_UART_RX
uint8_t UartBufEmpty(void)
{
    return !(rx_size);
}
#endif