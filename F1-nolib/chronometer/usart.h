/*
 * This file is part of the chronometer project.
 * Copyright 2019 Edward V. Emelianov <edward.emelianoff@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef __USART_H__
#define __USART_H__

#include <stm32f1.h>

// input and output buffers size
#define UARTBUFSZ   (128)
// timeout between data bytes
#ifndef TIMEOUT_MS
#define TIMEOUT_MS (1500)
#endif

// USART1 default speed
#define USART1_DEFAULT_SPEED    (115200)

#define STR_HELPER(s)   #s
#define STR(s)          STR_HELPER(s)

#ifdef EBUG
#define SEND(str) usart_send(1, str)
#define MSG(str)  do{SEND(__FILE__ " (L" STR(__LINE__) "): " str);}while(0)
#define DBG(str)  do{SEND(str); newline(); }while(0)
#else
#define SEND(str)
#define MSG(str)
#define DBG(str)
#endif

#define usartrx(n)  (linerdy[n])
#define usartovr(n) (bufovr[n])

extern volatile int linerdy[], bufovr[], txrdy[];

void transmit_tbuf(int n);
void usarts_setup();
int usart_getline(int n, char **line);
void usart_send(int n, const char *str);
void usart_putchar(int n, char ch);
void printu(int n, uint32_t val);
void printuhex(int n, uint32_t val);

#if defined EBUG || defined USART1PROXY
void newline();
#endif
#ifdef EBUG
void hexdump(uint8_t *arr, uint16_t len);
#endif

#endif // __USART_H__
