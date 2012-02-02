#ifndef GLOBAL_H
#define GLOBAL_H

#define BARE_LAUNCHPAD

#include "msp430.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include <assert.h>

#define TRUE 1
#define FALSE 0

// macros
#define _BV(bit) (1<<(bit))
#define SET_BIT(reg,bit)    ((reg)|= _BV(bit))
#define CLEAR_BIT(reg,bit)  ((reg)&=~_BV(bit))
#define TOGGLE_BIT(reg,bit) ((reg)^= _BV(bit))
#define WD_STOP()           (WDTCTL = WDTPW + WDTHOLD)
#define SET_CLOCK(N) BCSCTL1 = CALBC1_##N##MHZ; \
	                 DCOCTL = CALDCO_##N##MHZ;

//types
typedef unsigned char      uint8_t;
typedef signed char        int8_t;
typedef unsigned short int uint16_t;
typedef signed short int   int16_t;
typedef unsigned long int  uint32_t;
typedef signed long int    int32_t;

//macros for IO config (with preprocessor abuse)
#define st(x) do{x} while(__LINE__ == -1)

#define MAKE_OUTPUT(name)         _MAKE_OUTPUT(name##_PORT,name##_PIN)
#define MAKE_INPUT(name)          _MAKE_INPUT(name##_PORT,name##_PIN)
#define MAKE_SPECIAL(name)        _MAKE_SPECIAL(name##_PORT,name##_PIN)
#define SET_HIGH(name)            _SET_HIGH(name##_PORT,name##_PIN)
#define SET_LOW(name)             _SET_LOW(name##_PORT,name##_PIN)
#define TOGGLE(name)              _TOGGLE(name##_PORT,name##_PIN)
#define READ_IN(name)             _READ_IN(name##_PORT,name##_PIN)

#define _MAKE_OUTPUT(port,pin)    st(__MAKE_OUTPUT(port,pin);)
#define _MAKE_INPUT(port,pin)     st(__MAKE_INPUT(port,pin);)
#define _MAKE_SPECIAL(port, pin)  st(__MAKE_SPECIAL(port,pin);)
#define _SET_HIGH(port,pin)       st(__SET_HIGH(port,pin);)
#define _SET_LOW(port,pin)        st(__SET_LOW(port,pin);)
#define _TOGGLE(port,pin)         st(__TOGGLE(port,pin);)
#define _READ_IN(port,pin)        __READ_IN(port,pin)

#define __MAKE_OUTPUT(port,pin)   st((P##port##DIR |=  _BV(pin));)
#define __MAKE_INPUT(port, pin)   st((P##port##DIR &= ~_BV(pin));)
#define __MAKE_SPECIAL(port, pin) st((P##port##SEL |=  _BV(pin));)
#define __SET_HIGH(port,pin)      st((P##port##OUT |=  _BV(pin));)
#define __SET_LOW(port,pin)       st((P##port##OUT &= ~_BV(pin));)
#define __TOGGLE(port,pin)        st((P##port##OUT ^= _BV(pin));)
#define __READ_IN(port,pin)       !(P##port##IN & _BV(pin))

#endif
