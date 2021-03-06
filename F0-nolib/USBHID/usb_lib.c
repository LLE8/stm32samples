/*
 *                                                                                                  geany_encoding=koi8-r
 * usb_lib.c
 *
 * Copyright 2018 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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
 *
 */

#include <stdint.h>
#include "usb_lib.h"
#include <string.h> // memcpy
#include "usart.h"

static ep_t endpoints[ENDPOINTS_NUM];

//static uint8_t set_featuring;
static usb_dev_t USB_Dev;
static usb_LineCoding lineCoding = {115200, 0, 0, 8};
static config_pack_t setup_packet;
static uint8_t ep0databuf[EP0DATABUF_SIZE];
static uint8_t ep0dbuflen = 0;

usb_LineCoding getLineCoding(){return lineCoding;}

// definition of parts common for USB_DeviceDescriptor & USB_DeviceQualifierDescriptor
#define bcdUSB_L        0x00
#define bcdUSB_H        0x02
#define bDeviceClass    0
#define bDeviceSubClass 0
#define bDeviceProtocol 0
#define bNumConfigurations 1

static const uint8_t USB_DeviceDescriptor[] = {
        18,     // bLength
        0x01,   // bDescriptorType - Device descriptor
        bcdUSB_L,   // bcdUSB_L - 2.00
        bcdUSB_H,   // bcdUSB_H
        bDeviceClass,   // bDeviceClass - USB_COMM
        bDeviceSubClass,   // bDeviceSubClass
        bDeviceProtocol,   // bDeviceProtocol
        USB_EP0_BUFSZ,   // bMaxPacketSize0
        0x5e,   // idVendor: Microsoft
        0x04,   // idVendor_H
        0x5c,   // idProduct: Office Keyboard (106/109)
        0x00,   // idProduct_H
        0x00,   // bcdDevice_Ver_L
        0x02,   // bcdDevice_Ver_H
        0x01,   // iManufacturer
        0x02,   // iProduct
        0x03,   // iSerialNumber
        bNumConfigurations    // bNumConfigurations
};

static const uint8_t USB_DeviceQualifierDescriptor[] = {
        10,     //bLength
        0x06,   // bDescriptorType - Device qualifier
        bcdUSB_L,   // bcdUSB_L
        bcdUSB_H,   // bcdUSB_H
        bDeviceClass,   // bDeviceClass
        bDeviceSubClass,   // bDeviceSubClass
        bDeviceProtocol,   // bDeviceProtocol
        USB_EP0_BUFSZ,   // bMaxPacketSize0
        bNumConfigurations,   // bNumConfigurations
        0x00    // Reserved
};

static const uint8_t HID_ReportDescriptor[] = {
    0x05, 0x01, /* Usage Page (Generic Desktop)             */
    0x09, 0x02, /* Usage (Mouse)                            */
    0xA1, 0x01, /* Collection (Application)                 */
    0x09, 0x01, /*  Usage (Pointer)                         */
    0xA1, 0x00, /*  Collection (Physical)                   */
    0x85, 0x01,  /*   Report ID  */
    0x05, 0x09, /*      Usage Page (Buttons)                */
    0x19, 0x01, /*      Usage Minimum (01)                  */
    0x29, 0x03, /*      Usage Maximum (03)                  */
    0x15, 0x00, /*      Logical Minimum (0)                 */
    0x25, 0x01, /*      Logical Maximum (0)                 */
    0x95, 0x03, /*      Report Count (3)                    */
    0x75, 0x01, /*      Report Size (1)                     */
    0x81, 0x02, /*      Input (Data, Variable, Absolute)    */
    0x95, 0x01, /*      Report Count (1)                    */
    0x75, 0x05, /*      Report Size (5)                     */
    0x81, 0x01, /*      Input (Constant)    ;5 bit padding  */
    0x05, 0x01, /*      Usage Page (Generic Desktop)        */
    0x09, 0x30, /*      Usage (X)                           */
    0x09, 0x31, /*      Usage (Y)                           */
    0x15, 0x81, /*      Logical Minimum (-127)              */
    0x25, 0x7F, /*      Logical Maximum (127)               */
    0x75, 0x08, /*      Report Size (8)                     */
    0x95, 0x02, /*      Report Count (2)                    */
    0x81, 0x06, /*      Input (Data, Variable, Relative)    */
    0xC0, 0xC0,/* End Collection,End Collection            */
//
    0x09, 0x06, /*		Usage (Keyboard)                    */
    0xA1, 0x01, /*		Collection (Application)            */
    0x85, 0x02,  /*   Report ID  */
    0x05, 0x07, /*  	Usage (Key codes)                   */
    0x19, 0xE0, /*      Usage Minimum (224)                 */
    0x29, 0xE7, /*      Usage Maximum (231)                 */
    0x15, 0x00, /*      Logical Minimum (0)                 */
    0x25, 0x01, /*      Logical Maximum (1)                 */
    0x75, 0x01, /*      Report Size (1)                     */
    0x95, 0x08, /*      Report Count (8)                    */
    0x81, 0x02, /*      Input (Data, Variable, Absolute)    */
    0x95, 0x01, /*      Report Count (1)                    */
    0x75, 0x08, /*      Report Size (8)                     */
    0x81, 0x01, /*      Input (Constant)    ;5 bit padding  */
    0x95, 0x05, /*      Report Count (5)                    */
    0x75, 0x01, /*      Report Size (1)                     */
    0x05, 0x08, /*      Usage Page (Page# for LEDs)         */
    0x19, 0x01, /*      Usage Minimum (01)                  */
    0x29, 0x05, /*      Usage Maximum (05)                  */
    0x91, 0x02, /*      Output (Data, Variable, Absolute)   */
    0x95, 0x01, /*      Report Count (1)                    */
    0x75, 0x03, /*      Report Size (3)                     */
    0x91, 0x01, /*      Output (Constant)                   */
    0x95, 0x06, /*      Report Count (1)                    */
    0x75, 0x08, /*      Report Size (3)                     */
    0x15, 0x00, /*      Logical Minimum (0)                 */
    0x25, 0x65, /*      Logical Maximum (101)               */
    0x05, 0x07, /*  	Usage (Key codes)                   */
    0x19, 0x00, /*      Usage Minimum (00)                  */
    0x29, 0x65, /*      Usage Maximum (101)                 */
    0x81, 0x00, /*      Input (Data, Array)                 */
    0xC0        /* 		End Collection,End Collection       */
};

#if 0
const uint8_t HID_ReportDescriptor[] = {
    0x05, 0x01, /* Usage Page (Generic Desktop)             */
    0x09, 0x06, /*      Usage (Keyboard)                    */
    0xA1, 0x01, /*      Collection (Application)            */
    0x05, 0x07, /*      Usage (Key codes)                   */
    0x19, 0xE0, /*      Usage Minimum (224)                 */
    0x29, 0xE7, /*      Usage Maximum (231)                 */
    0x15, 0x00, /*      Logical Minimum (0)                 */
    0x25, 0x01, /*      Logical Maximum (1)                 */
    0x75, 0x01, /*      Report Size (1)                     */
    0x95, 0x08, /*      Report Count (8)                    */
    0x81, 0x02, /*      Input (Data, Variable, Absolute)    */
    0x95, 0x01, /*      Report Count (1)                    */
    0x75, 0x08, /*      Report Size (8)                     */
    0x81, 0x01, /*      Input (Constant)    ;5 bit padding  */
    0x95, 0x05, /*      Report Count (5)                    */
    0x75, 0x01, /*      Report Size (1)                     */
    0x05, 0x08, /*      Usage Page (Page# for LEDs)         */
    0x19, 0x01, /*      Usage Minimum (01)                  */
    0x29, 0x05, /*      Usage Maximum (05)                  */
    0x91, 0x02, /*      Output (Data, Variable, Absolute)   */
    0x95, 0x01, /*      Report Count (1)                    */
    0x75, 0x03, /*      Report Size (3)                     */
    0x91, 0x01, /*      Output (Constant)                   */
    0x95, 0x06, /*      Report Count (1)                    */
    0x75, 0x08, /*      Report Size (3)                     */
    0x15, 0x00, /*      Logical Minimum (0)                 */
    0x25, 0x65, /*      Logical Maximum (101)               */
    0x05, 0x07, /*      Usage (Key codes)                   */
    0x19, 0x00, /*      Usage Minimum (00)                  */
    0x29, 0x65, /*      Usage Maximum (101)                 */
    0x81, 0x00, /*      Input (Data, Array)                 */
    0x09, 0x05, /*      Usage (Vendor Defined)              */
    0x15, 0x00, /*      Logical Minimum (0))                */
    0x26, 0xFF, 0x00, /* Logical Maximum (255))             */
    0x75, 0x08, /*      Report Count (2))                   */
    0x95, 0x02, /*      Report Size (8 bit))                */
    0xB1, 0x02, /*      Feature (Data, Variable, Absolute)  */
    0xC0        /*      End Collection,End Collection       */

};
#endif

static const uint8_t USB_ConfigDescriptor[] = {
        /*Configuration Descriptor*/
        0x09, /* bLength: Configuration Descriptor size */
        0x02, /* bDescriptorType: Configuration */
        34,   /* wTotalLength */
        0x00,
        0x01, /* bNumInterfaces: 1 interface */
        0x01, /* bConfigurationValue: Configuration value */
        0x00, /* iConfiguration: Index of string descriptor describing the configuration */
        0xa0, /* bmAttributes - Bus powered */
        0x32, /* MaxPower 100 mA */
        /*Interface Descriptor */
        0x09, /* bLength: Interface Descriptor size */
        0x04, /* bDescriptorType: Interface */
        0x00, /* bInterfaceNumber: Number of Interface */
        0x00, /* bAlternateSetting: Alternate setting */
        0x01, /* bNumEndpoints: 1 endpoint used */
        0x03, /* bInterfaceClass: USB_CLASS_HID */
        0x01, /* bInterfaceSubClass: boot */
        0x01, /* bInterfaceProtocol: keyboard */
        0x00, /* iInterface: */
        /* HID device descriptor */
        0x09, /* bLength: HID Device Descriptor size */
        0x21, /* bDescriptorType: HID */
        0x10, /* bcdHID: 1.10 */
        0x01, /* bcdHIDH */
        0x00, /* bCountryCode: Not supported */
        0x01, /* bNumDescriptors: 1 */
        0x22, /* bDescriptorType: Report */
        sizeof(HID_ReportDescriptor), /* wDescriptorLength */
        0x00, /*  wDescriptorLengthH */
        /*Endpoint 1 Descriptor*/
        0x07, /* bLength: Endpoint Descriptor size */
        0x05, /* bDescriptorType: Endpoint */
        0x81, /* bEndpointAddress IN1 */
        0x03, /* bmAttributes: Interrupt */
        USB_TXBUFSZ, /* wMaxPacketSize LO: */
        0x00, /* wMaxPacketSize HI: */
        0x01, /* bInterval: */
};

_USB_LANG_ID_(USB_StringLangDescriptor, LANG_US);
// these descriptors are not used in PL2303 emulator!
_USB_STRING_(USB_StringSerialDescriptor, u"0");
_USB_STRING_(USB_StringManufacturingDescriptor, u"SAO RAS");
_USB_STRING_(USB_StringProdDescriptor, u"HID mouse+keyboard");

#ifdef EBUG
    uint8_t _2wr = 0;
    #define WRITEDUMP(str)  do{MSG(str); _2wr = 1;}while(0)
#else
    #define WRITEDUMP(str)
#endif
static void wr0(const uint8_t *buf, uint16_t size){
    if(setup_packet.wLength < size) size = setup_packet.wLength;
    EP_WriteIRQ(0, buf, size);
}

static inline void get_descriptor(){
    switch(setup_packet.wValue){
        case DEVICE_DESCRIPTOR:
            WRITEDUMP("DEVICE_DESCRIPTOR");
            wr0(USB_DeviceDescriptor, sizeof(USB_DeviceDescriptor));
        break;
        case CONFIGURATION_DESCRIPTOR:
            WRITEDUMP("CONFIGURATION_DESCRIPTOR");
            wr0(USB_ConfigDescriptor, sizeof(USB_ConfigDescriptor));
        break;
        case STRING_LANG_DESCRIPTOR:
            WRITEDUMP("STRING_LANG_DESCRIPTOR");
            wr0((const uint8_t *)&USB_StringLangDescriptor, STRING_LANG_DESCRIPTOR_SIZE_BYTE);
        break;
        case STRING_MAN_DESCRIPTOR:
            WRITEDUMP("STRING_MAN_DESCRIPTOR");
            wr0((const uint8_t *)&USB_StringManufacturingDescriptor, USB_StringManufacturingDescriptor.bLength);
        break;
        case STRING_PROD_DESCRIPTOR:
            WRITEDUMP("STRING_PROD_DESCRIPTOR");
            wr0((const uint8_t *)&USB_StringProdDescriptor, USB_StringProdDescriptor.bLength);
        break;
        case STRING_SN_DESCRIPTOR:
            WRITEDUMP("STRING_SN_DESCRIPTOR");
            wr0((const uint8_t *)&USB_StringSerialDescriptor, USB_StringSerialDescriptor.bLength);
        break;
        case DEVICE_QUALIFIER_DESCRIPTOR:
            WRITEDUMP("DEVICE_QUALIFIER_DESCRIPTOR");
            wr0(USB_DeviceQualifierDescriptor, USB_DeviceQualifierDescriptor[0]);
        break;
        default:
            WRITEDUMP("UNK_DES");
        break;
    }
}

static uint8_t configuration = 0; // reply for GET_CONFIGURATION (==1 if configured)
static inline void std_d2h_req(){
    uint16_t status = 0; // bus powered
    switch(setup_packet.bRequest){
        case GET_DESCRIPTOR:
            get_descriptor();
        break;
        case GET_STATUS:
            WRITEDUMP("GET_STATUS");
            EP_WriteIRQ(0, (uint8_t *)&status, 2); // send status: Bus Powered
        break;
        case GET_CONFIGURATION:
            WRITEDUMP("GET_CONFIGURATION");
            EP_WriteIRQ(0, &configuration, 1);
        break;
        default:
            WRITEDUMP("80:WR_REQ");
        break;
    }
}

static inline void std_h2d_req(){
    switch(setup_packet.bRequest){
        case SET_ADDRESS:
            WRITEDUMP("SET_ADDRESS");
            // new address will be assigned later - after  acknowlegement or request to host
            USB_Dev.USB_Addr = setup_packet.wValue;
        break;
        case SET_CONFIGURATION:
            WRITEDUMP("SET_CONFIGURATION");
            // Now device configured
            USB_Dev.USB_Status = USB_CONFIGURE_STATE;
            configuration = setup_packet.wValue;
        break;
        default:
            WRITEDUMP("0:WR_REQ");
        break;
    }
}

static uint16_t WriteHID_descriptor(uint16_t status){
    uint16_t rest = sizeof(HID_ReportDescriptor);
    uint8_t *ptr = (uint8_t*)HID_ReportDescriptor;
    while(rest){
        uint16_t l = rest;
        if(l > endpoints[0].txbufsz) l = endpoints[0].txbufsz;
        EP_WriteIRQ(0, ptr, l);
        ptr += l;
        rest -= l;
        MSG("Sent\n");
        uint8_t needzlp = (l == endpoints[0].txbufsz) ? 1 : 0;
        if(rest || needzlp){ // send last data buffer
            status = SET_NAK_RX(status);
            status = SET_VALID_TX(status);
            status = KEEP_DTOG_TX(status);
            status = KEEP_DTOG_RX(status);
            status = CLEAR_CTR_RX(status);
            status = CLEAR_CTR_TX(status);
            USB->ISTR = 0;
            USB->EPnR[0] = status;
            uint32_t ctr = 1000000;
            while(--ctr && (USB->ISTR & USB_ISTR_CTR) == 0);
            if((USB->ISTR & USB_ISTR_CTR) == 0){MSG("ERR\n")};
            USB->ISTR = 0;
            status = USB->EPnR[0];
            if(needzlp) EP_WriteIRQ(0, (uint8_t*)0, 0);
        }
    }
    return status;
}

/*
bmRequestType: 76543210
7    direction: 0 - host->device, 1 - device->host
65   type: 0 - standard, 1 - class, 2 - vendor
4..0 getter: 0 - device, 1 - interface, 2 - endpoint, 3 - other
*/
/**
 * Endpoint0 (control) handler
 * @param ep - endpoint state
 * @return data written to EP0R
 */
static uint16_t EP0_Handler(ep_t ep){
    uint16_t epstatus = ep.status; // EP0R on input -> return this value after modifications
    uint8_t reqtype = setup_packet.bmRequestType & 0x7f;
    uint8_t dev2host = (setup_packet.bmRequestType & 0x80) ? 1 : 0;
    if ((ep.rx_flag) && (ep.setup_flag)){
        switch(reqtype){
            case STANDARD_DEVICE_REQUEST_TYPE: // standard device request
                if(dev2host){
                    std_d2h_req();
                }else{
                    std_h2d_req();
                    // send ZLP
                    EP_WriteIRQ(0, (uint8_t *)0, 0);
                }
                epstatus = SET_NAK_RX(epstatus);
                epstatus = SET_VALID_TX(epstatus);
            break;
            case STANDARD_INTERFACE_REQUEST_TYPE:
                if(dev2host && setup_packet.bRequest == GET_DESCRIPTOR){
                    if(setup_packet.wValue == HID_REPORT_DESCRIPTOR){
                        WRITEDUMP("HID_REPORT");
                        epstatus = WriteHID_descriptor(epstatus);
                    }
                }
                epstatus = SET_NAK_RX(epstatus);
                epstatus = SET_VALID_TX(epstatus);
            break;
            case STANDARD_ENDPOINT_REQUEST_TYPE: // standard endpoint request
                if (setup_packet.bRequest == CLEAR_FEATURE){
                    printu(setup_packet.wValue);
                    //WRITEDUMP("CLEAR_FEATURE");
                    // send ZLP
                    EP_WriteIRQ(0, (uint8_t *)0, 0);
                    epstatus = SET_NAK_RX(epstatus);
                    epstatus = SET_VALID_TX(epstatus);
                }else{
                    WRITEDUMP("02:WR_REQ");
                }
            break;
            case CONTROL_REQUEST_TYPE:
                if (setup_packet.bRequest == SET_IDLE_REQUEST){
                    EP_WriteIRQ(0, (uint8_t *)0, 0);
                    epstatus = SET_NAK_RX(epstatus);
                    epstatus = SET_VALID_TX(epstatus);
                    WRITEDUMP("SET_IDLE_REQUEST");
                } else if (setup_packet.bRequest == SET_FEAUTRE){
                    WRITEDUMP("SET_FEAUTRE");
                    //set_featuring = 1;
                    epstatus = SET_VALID_RX(epstatus);
                    epstatus = KEEP_STAT_TX(epstatus);
                }
            break;
            default:
                WRITEDUMP("Bad request");
                EP_WriteIRQ(0, (uint8_t *)0, 0);
                epstatus = SET_NAK_RX(epstatus);
                epstatus = SET_VALID_TX(epstatus);
        }
    }else if (ep.rx_flag || ep.tx_flag){ // got data over EP0 or host acknowlegement || package transmitted
        if(ep.rx_flag){
            /*if (set_featuring){
                set_featuring = 0;
                // here we can do something with ep.rx_buf - set_feature
            }*/
            // Close transaction
#ifdef EBUG
            hexdump(ep.rx_buf, ep.rx_cnt);
#endif
        }else{ // tx
            // now we can change address after enumeration
            if ((USB->DADDR & USB_DADDR_ADD) != USB_Dev.USB_Addr){
                USB->DADDR = USB_DADDR_EF | USB_Dev.USB_Addr;
                // change state to ADRESSED
                USB_Dev.USB_Status = USB_ADRESSED_STATE;
            }
        }
        // end of transaction
        epstatus = CLEAR_DTOG_RX(epstatus);
        epstatus = CLEAR_DTOG_TX(epstatus);
        epstatus = SET_VALID_RX(epstatus);
        epstatus = SET_VALID_TX(epstatus);
    }
#ifdef EBUG
    if(_2wr){
        usart_putchar(' ');
        if (ep.rx_flag) usart_putchar('r');
        else usart_putchar('t');
        printu(setup_packet.wLength);
        if(ep.setup_flag) usart_putchar('s');
        usart_putchar(' ');
        usart_putchar('I');
        printu(setup_packet.wIndex);
        usart_putchar('V');
        printu(setup_packet.wValue);
        usart_putchar('R');
        printu(setup_packet.bRequest);
        usart_putchar('T');
        printu(setup_packet.bmRequestType);
        usart_putchar(' ');
        usart_putchar('0' + ep0dbuflen);
        usart_putchar(' ');
        hexdump(ep0databuf, ep0dbuflen);
        usart_putchar('\n');
        _2wr = 0;
    }
#endif
    return epstatus;
}
#undef WRITEDUMP

static uint16_t lastaddr = USB_EP0_BASEADDR;
/**
 * Endpoint initialisation
 * !!! when working with CAN bus change USB_BTABLE_SIZE to 768 !!!
 * @param number - EP num (0...7)
 * @param type - EP type (EP_TYPE_BULK, EP_TYPE_CONTROL, EP_TYPE_ISO, EP_TYPE_INTERRUPT)
 * @param txsz - transmission buffer size @ USB/CAN buffer
 * @param rxsz - reception buffer size @ USB/CAN buffer
 * @param uint16_t (*func)(ep_t *ep) - EP handler function
 * @return 0 if all OK
 */
int EP_Init(uint8_t number, uint8_t type, uint16_t txsz, uint16_t rxsz, uint16_t (*func)(ep_t ep)){
    if(number >= ENDPOINTS_NUM) return 4; // out of configured amount
    if(txsz > USB_BTABLE_SIZE || rxsz > USB_BTABLE_SIZE) return 1; // buffer too large
    if(lastaddr + txsz + rxsz >= USB_BTABLE_SIZE) return 2; // out of btable
    USB->EPnR[number] = (type << 9) | (number & USB_EPnR_EA);
    USB->EPnR[number] ^= USB_EPnR_STAT_RX | USB_EPnR_STAT_TX_1;
    if(rxsz & 1 || rxsz > 992) return 3; // wrong rx buffer size
    uint16_t countrx = 0;
    if(rxsz < 64) countrx = rxsz / 2;
    else{
        if(rxsz & 0x1f) return 3; // should be multiple of 32
        countrx = 31 + rxsz / 32;
    }
    USB_BTABLE->EP[number].USB_ADDR_TX = lastaddr;
    endpoints[number].tx_buf = (uint16_t *)(USB_BTABLE_BASE + lastaddr);
    endpoints[number].txbufsz = txsz;
    lastaddr += txsz;
    USB_BTABLE->EP[number].USB_COUNT_TX = 0;
    USB_BTABLE->EP[number].USB_ADDR_RX = lastaddr;
    endpoints[number].rx_buf = (uint8_t *)(USB_BTABLE_BASE + lastaddr);
    lastaddr += rxsz;
    // buffer size: Table127 of RM
    USB_BTABLE->EP[number].USB_COUNT_RX = countrx << 10;
    endpoints[number].func = func;
    return 0;
}

// standard IRQ handler
void usb_isr(){
    // disallow interrupts
    USB->CNTR = 0;
    if (USB->ISTR & USB_ISTR_RESET){
        USB->ISTR = 0;
        // Endpoint 0 - CONTROL
        // ON USB LS size of EP0 may be 8 bytes, but on FS it should be 64 bytes!
        lastaddr = USB_EP0_BASEADDR; // roll back to beginning of buffer
        EP_Init(0, EP_TYPE_CONTROL, USB_EP0_BUFSZ, USB_EP0_BUFSZ, EP0_Handler);
        // clear address, leave only enable bit
        USB->DADDR = USB_DADDR_EF;
        // state is default - wait for enumeration
        USB_Dev.USB_Status = USB_DEFAULT_STATE;
    }
    if(USB->ISTR & USB_ISTR_CTR){
        // EP number
        uint8_t n = USB->ISTR & USB_ISTR_EPID;
        // copy status register
        uint16_t epstatus = USB->EPnR[n];
        // Calculate flags
        endpoints[n].rx_flag = (epstatus & USB_EPnR_CTR_RX) ? 1 : 0;
        endpoints[n].setup_flag = (epstatus & USB_EPnR_SETUP) ? 1 : 0;
        endpoints[n].tx_flag = (epstatus & USB_EPnR_CTR_TX) ? 1 : 0;
        // copy received bytes amount
        endpoints[n].rx_cnt = USB_BTABLE->EP[n].USB_COUNT_RX & 0x3FF; // low 10 bits is counter
        // check direction
        if(USB->ISTR & USB_ISTR_DIR){ // OUT interrupt - receive data, CTR_RX==1 (if CTR_TX == 1 - two pending transactions: receive following by transmit)
            if(n == 0){ // control endpoint
                if(epstatus & USB_EPnR_SETUP){ // setup packet -> copy data to conf_pack
                    memcpy(&setup_packet, endpoints[0].rx_buf, sizeof(setup_packet));
                    ep0dbuflen = 0;
                    // interrupt handler will be called later
                }else if(epstatus & USB_EPnR_CTR_RX){ // data packet -> push received data to ep0databuf
                    ep0dbuflen = endpoints[0].rx_cnt;
                    memcpy(ep0databuf, endpoints[0].rx_buf, ep0dbuflen);
                }
            }
        }else{ // IN interrupt - transmit data, only CTR_TX == 1
            // enumeration end could be here (if EP0)
        }
        // prepare status field for EP handler
        endpoints[n].status = epstatus;
        // call EP handler (even if it will change EPnR, it should return new status)
        epstatus = endpoints[n].func(endpoints[n]);
        // keep DTOG state
        epstatus = KEEP_DTOG_TX(epstatus);
        epstatus = KEEP_DTOG_RX(epstatus);
        // clear all RX/TX flags
        epstatus = CLEAR_CTR_RX(epstatus);
        epstatus = CLEAR_CTR_TX(epstatus);
        // refresh EPnR
        USB->EPnR[n] = epstatus;
    }
    // allow interrupts
    USB->CNTR = USB_CNTR_RESETM | USB_CNTR_CTRM;
}

/**
 * Write data to EP buffer (called from IRQ handler)
 * @param number - EP number
 * @param *buf - array with data
 * @param size - its size
 */
void EP_WriteIRQ(uint8_t number, const uint8_t *buf, uint16_t size){
    uint8_t i;
    if(size > endpoints[number].txbufsz) size = endpoints[number].txbufsz;
    uint16_t N2 = (size + 1) >> 1;
    // the buffer is 16-bit, so we should copy data as it would be uint16_t
    uint16_t *buf16 = (uint16_t *)buf;
    for (i = 0; i < N2; i++){
        endpoints[number].tx_buf[i] = buf16[i];
    }
    USB_BTABLE->EP[number].USB_COUNT_TX = size;
}

/**
 * Write data to EP buffer (called outside IRQ handler)
 * @param number - EP number
 * @param *buf - array with data
 * @param size - its size
 */
void EP_Write(uint8_t number, const uint8_t *buf, uint16_t size){
    uint16_t status = USB->EPnR[number];
    EP_WriteIRQ(number, buf, size);
    status = SET_NAK_RX(status);
    status = SET_VALID_TX(status);
    status = KEEP_DTOG_TX(status);
    status = KEEP_DTOG_RX(status);
    USB->EPnR[number] = status;
}

/*
 * Copy data from EP buffer into user buffer area
 * @param *buf - user array for data
 * @return amount of data read
 */
int EP_Read(uint8_t number, uint8_t *buf){
    int n = endpoints[number].rx_cnt;
    if(n){
        for(int i = 0; i < n; ++i)
            buf[i] = endpoints[number].rx_buf[i];
    }
    return n;
}

// USB status
uint8_t USB_GetState(){
    return USB_Dev.USB_Status;
}
