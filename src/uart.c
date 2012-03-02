#include "global.h"
#include "uart.h"
#include "hardware.h"

////////////////////////////////////////////////////////////////////////////////
//                              INIT
////////////////////////////////////////////////////////////////////////////////
// see userguide table 15-5
static const BaudRateConfig rate_table[5] = {{  9600, 16000000, 104, 0,  3},
                                             { 19200, 16000000,  52, 0,  1},
                                             { 38400, 16000000,  26, 0,  1},
                                             { 57600, 16000000,  17, 0,  6},
                                             {115200, 16000000,   8, 0, 11}};
void UartInit(uint32_t baud_rate)
{
    uint8_t i = 0;
    uint8_t index = 0;
    // find where the baud rate is in the table
    for(i = 0;i < sizeof(rate_table) / sizeof(BaudRateConfig);i++)
    {
        if (baud_rate == rate_table[i].baud && CLOCK_DCO == rate_table[i].clock)
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
    // UCBRS is at bits 4-7
    UCA0MCTL |= rate_table[index].ucbrf << USBRF_OFFSET;
    // set UCOS16 to enable oversampling
    UCA0MCTL |= UCOS16;

    // Initialize USCI state machine
    UCA0CTL1 &= ~UCSWRST;
    // Enable USCI_A0 RX and TX interrupts
#ifdef NON_BLOCKING_UART_TX
    IE2 |= UCA0TXIE;
#endif
#ifdef NON_BLOCKING_UART_RX
    IE2 |= UCA0RXIE;
#endif
}

////////////////////////////////////////////////////////////////////////////////
//                              Receive Queueing
////////////////////////////////////////////////////////////////////////////////
#ifdef NON_BLOCKING_UART_RX
#if MAX_UART_RX_BUF_CNT > 0
static uint8_t rx_start = 0;
static uint8_t rx_size  = 0;
static uint8_t rx_buf[MAX_UART_RX_BUF_CNT];
static int8_t UartRxBufferEnqueue(uint8_t data)
{
    // is the buffer full?
    if (rx_size == MAX_UART_RX_BUF_CNT)
    {
        goto error;
    }

    // stuff the data
    rx_buf[((rx_start + rx_size) == MAX_UART_RX_BUF_CNT) ?
                                    0 :
                                    (rx_start + rx_size)] = data;

    // adjust the buf size
    rx_size++;

    return (1);
error:
    return (-1);
}

uint8_t UartRxBufferDequeue(uint8_t *in, uint16_t len)
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
    rx_start = ((rx_start + rx_size) >= MAX_UART_RX_BUF_CNT) ?
               ((rx_start + len_to_read) - MAX_UART_RX_BUF_CNT) :
                (rx_start + len_to_read);

    // adjust the size
    rx_size -= len_to_read;

    _EINT();
    return len_to_read;
error:
    _EINT();
    return (0);
}

uint8_t get_uart_rx_buf_size(void)
{
    return rx_size;
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void UartRxInt(void)
{
    UartRxBufferEnqueue(UCA0RXBUF);
}

#endif // MAX_UART_RX_BUF_CNT > 0
#else  // NON_BLOCKING_UART_RX
uint8_t UartGetC(void)
{
    while (!(IFG2 & UCA0RXIFG));
    return UCA0RXBUF;
}
#endif // NON_BLOCKING_UART_RX
////////////////////////////////////////////////////////////////////////////////
//                              Transmit Queueing
////////////////////////////////////////////////////////////////////////////////
#if MAX_UART_TX_BUF_CNT > 0
#ifdef NON_BLOCKING_UART_TX
static uint8_t tx_start = 0;
static uint8_t tx_size  = 0;
static uint8_t tx_buf[MAX_UART_TX_BUF_CNT];
static int8_t UartTxBufferEnqueue(uint8_t data)
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
    return (1);
error:
    _EINT();
    return (-1);
}

static uint8_t UartTxBufferDequeue(void)
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

#pragma vector = USCIAB0TX_VECTOR
__interrupt void UartTxInt(void)
{
    UCA0TXBUF = UartTxBufferDequeue();
}

#endif //NON_BLOCKING_UART_TX
#endif //MAX_UART_TX_BUF_CNT > 0

////////////////////////////////////////////////////////////////////////////////
//                              Printf implementation
////////////////////////////////////////////////////////////////////////////////
// Code adapted from 43oh.com forums

static void UartPutC(uint8_t data)
{
#ifdef NON_BLOCKING_UART_TX
    UartTxBufferEnqueue(data);
#else
    while (!(IFG2 & UCA0TXIFG));    // TX buffer ready?
    UCA0TXBUF = data;
#endif
}

static void UartPutS(uint8_t *s)
{
    while(*s)
    {
        UartPutC(*s++);
    }
}

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
const uint8_t char_table[] = "0123456789ABCDEF";

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


static void UartPutAToI(uint32_t value, enum NUMBER_BASE base)
{
    uint32_t position = 0;
    uint8_t index = 0;

    // pointer to which table we're using
    const uint32_t* selected_table;
    selected_table = (base == HEX) ? hex_table : decimal_table;

    // work out way down to the current decade within that table
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
                case 'i':   // 16 bit signed
                case 'u':   // 16 bit unsigned
                    current_short_int = va_arg(arg_list, uint16_t);
                    // if we're negative
                    if(current_char == 'i' && current_short_int < 0)
                    {
                        // add the negative sign
                        UartPutC('-');
                        // swap the value to positive to print
                        current_short_int = -current_short_int;

                    }
                    UartPutAToI(current_short_int, DECIMAL);
                    break;
                case 'l':   // 32 bit signed
                case 'n':   // 32 bit unsigned
                    current_long_int = va_arg(arg_list, uint32_t);
                    if(current_char == 'l' &&  current_long_int < 0)
                    {
                        // add the negative sign
                        UartPutC('-');
                        // swap the value to positive to print
                        current_long_int = -current_long_int;
                    }
                    UartPutAToI(current_long_int, DECIMAL);
                    break;
                case 'x':   // 16 or 32 bit hex
                    current_long_int = va_arg(arg_list, uint32_t);
                    UartPutAToI(current_long_int, HEX);
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