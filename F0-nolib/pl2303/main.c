/*
 * main.c
 *
 * Copyright 2017 Edward V. Emelianoff <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#include "hardware.h"
#include "usart.h"
#include "usb.h"
#include "usb_lib.h"
#include <string.h> // memcpy

volatile uint32_t Tms = 0;

/* Called when systick fires */
void sys_tick_handler(void){
    ++Tms;
}

void iwdg_setup(){
    uint32_t tmout = 16000000;
    /* Enable the peripheral clock RTC */
    /* (1) Enable the LSI (40kHz) */
    /* (2) Wait while it is not ready */
    RCC->CSR |= RCC_CSR_LSION; /* (1) */
    while((RCC->CSR & RCC_CSR_LSIRDY) != RCC_CSR_LSIRDY){if(--tmout == 0) break;} /* (2) */
    /* Configure IWDG */
    /* (1) Activate IWDG (not needed if done in option bytes) */
    /* (2) Enable write access to IWDG registers */
    /* (3) Set prescaler by 64 (1.6ms for each tick) */
    /* (4) Set reload value to have a rollover each 2s */
    /* (5) Check if flags are reset */
    /* (6) Refresh counter */
    IWDG->KR = IWDG_START; /* (1) */
    IWDG->KR = IWDG_WRITE_ACCESS; /* (2) */
    IWDG->PR = IWDG_PR_PR_1; /* (3) */
    IWDG->RLR = 1250; /* (4) */
    tmout = 16000000;
    while(IWDG->SR){if(--tmout == 0) break;} /* (5) */
    IWDG->KR = IWDG_REFRESH; /* (6) */
}

static usb_LineCoding new_lc;
static uint8_t lcchange = 0;
static void show_new_lc(){
    SEND("got new linecoding:");
    SEND(" baudrate="); printu(new_lc.dwDTERate);
    SEND(", charFormat="); printu(new_lc.bCharFormat);
    SEND(" (");
    switch(new_lc.bCharFormat){
        case USB_CDC_1_STOP_BITS:
            usart_putchar('1');
        break;
        case USB_CDC_1_5_STOP_BITS:
            SEND("1.5");
        break;
        case USB_CDC_2_STOP_BITS:
            usart_putchar('2');
        break;
        default:
            usart_putchar('?');
    }
    SEND(" stop bits), parityType="); printu(new_lc.bParityType);
    SEND(" (");
    switch(new_lc.bParityType){
        case USB_CDC_NO_PARITY:
            SEND("no");
        break;
        case USB_CDC_ODD_PARITY:
            SEND("odd");
        break;
        case USB_CDC_EVEN_PARITY:
            SEND("even");
        break;
        case USB_CDC_MARK_PARITY:
            SEND("mark");
        break;
        case USB_CDC_SPACE_PARITY:
            SEND("space");
        break;
        default:
            SEND("unknown");
    }
    SEND(" parity), dataBits="); printu(new_lc.bDataBits);
    usart_putchar('\n');
    lcchange = 0;
}

void linecoding_handler(usb_LineCoding *lc){
    memcpy(&new_lc, lc, sizeof(usb_LineCoding));
    lcchange = 1;
}

void clstate_handler(uint16_t val){
    SEND("change control line state to ");
    printu(val);
    if(val & CONTROL_DTR) SEND(" (DTR)");
    if(val & CONTROL_RTS) SEND(" (RTS)");
    usart_putchar('\n');
}

int main(void){
    uint32_t lastT = 0;
    int L = 0;
    char *txt;
    char tmpbuf[129];
    sysreset();
    SysTick_Config(6000, 1);
    gpio_setup();
    usart_setup();

    SEND("Hello!\n");

    if(RCC->CSR & RCC_CSR_IWDGRSTF){ // watchdog reset occured
        SEND("WDGRESET=1\n");
    }
    if(RCC->CSR & RCC_CSR_SFTRSTF){ // software reset occured
        SEND("SOFTRESET=1\n");
    }
    RCC->CSR |= RCC_CSR_RMVF; // remove reset flags

    USB_setup();
    iwdg_setup();

    while (1){
        IWDG->KR = IWDG_REFRESH; // refresh watchdog
        if(lastT > Tms || Tms - lastT > 499){
            LED_blink(LED0);
            lastT = Tms;
            transmit_tbuf(); // non-blocking transmission of data from UART buffer every 0.5s
        }
        usb_proc();
        uint8_t r = 0;
        if((r = USB_receive(tmpbuf, 128))){
            tmpbuf[r] = 0;
            SEND("Received data over USB:\n");
            SEND(tmpbuf);
            newline();
        }
        if(usartrx()){ // usart1 received data, store in in buffer
            L = usart_getline(&txt);
            char _1st = txt[0];
            if(L == 2 && txt[1] == '\n'){
                L = 0;
                switch(_1st){
                    case 'C':
                        SEND("USB ");
                        if(!USB_configured()) SEND("dis");
                        SEND("connected\n");
                    break;
                    case 'L':
                        USB_send("Very long test string for USB (it's length is more than 64 bytes\n"
                                 "This is another part of the string! Can you see all of this?\n");
                        SEND("Long test sent\n");
                    break;
                    case 'R':
                        SEND("Soft reset\n");
                        NVIC_SystemReset();
                    break;
                    case 'S':
                        USB_send("Test string for USB\n");
                        SEND("Short test sent\n");
                    break;
                    case 'W':
                        SEND("Wait for reboot\n");
                        while(1){nop();};
                    break;
                    default: // help
                        SEND(
                        "'C' - test if USB is configured\n"
                        "'L' - send long string over USB\n"
                        "'R' - software reset\n"
                        "'S' - send short string over USB\n"
                        "'W' - test watchdog\n"
                        );
                    break;
                }
            }
            transmit_tbuf();
        }
        if(L){ // echo all other data
            txt[L] = 0;
            usart_send(txt);
            L = 0;
        }
        if(lcchange){
            show_new_lc();
        }
    }
    return 0;
}

