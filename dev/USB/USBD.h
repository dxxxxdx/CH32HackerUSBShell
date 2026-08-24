#ifndef CH32V203C8U_USBD_H
#define CH32V203C8U_USBD_H

#include <stdint.h>


#define BSP_USB_EP0_PACKET_SIZE 64U


uint8_t USBD_IsHostConfigured(void);

uint8_t USBD_IsDataInterfaceEnabled(void);


#endif /* CH32V203C8U_USBD_H */
