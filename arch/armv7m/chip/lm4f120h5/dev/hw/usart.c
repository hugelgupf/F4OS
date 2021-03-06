/*
 * Copyright (C) 2013, 2014 F4OS Authors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arch/system.h>
#include <arch/chip/registers.h>
#include <arch/chip/rom.h>
#include <dev/char.h>
#include <dev/device.h>
#include <dev/resource.h>
#include <dev/hw/usart.h>
#include <kernel/fault.h>
#include <kernel/init.h>
#include <kernel/mutex.h>
#include <kernel/sched.h>
#include <mm/mm.h>

struct mutex usart_read_mutex = INIT_MUTEX;
struct mutex usart_write_mutex = INIT_MUTEX;

resource usart_resource = { .writer     = &usart_putc,
                            .swriter    = &usart_puts,
                            .reader     = &usart_getc,
                            .closer     = &usart_close,
                            .env        = NULL,
                            .read_mut   = &usart_read_mutex,
                            .write_mut  = &usart_write_mutex};

static void usart_baud(uint32_t baud) __attribute__((section(".kernel")));

uint8_t usart_ready = 0;

int init_usart(void) {
    /* Enable UART0 clock */
    SYSCTL_RCGC1_R |= SYSCTL_RCGC1_UART0;

    /* Enable GPIOA clock */
    SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOA;

    ROM_SysCtlDelay(1);

    /* Alternate functions UART */
    GPIO_PORTA_PCTL_R |= (1 << 4) | (1 << 0);

    /* Set to output */
    GPIO_PORTA_DIR_R &= ~(GPIO_PIN_1 | GPIO_PIN_0);

    /* Set PA0 and PA1 to alternate function */
    GPIO_PORTA_AFSEL_R |= GPIO_PIN_1 | GPIO_PIN_0;

    /* Digital pins */
    GPIO_PORTA_DEN_R |= GPIO_PIN_1 | GPIO_PIN_0;

    /* Disable UART */
    UART0_CTL_R &= ~(UART_CTL_UARTEN);

    usart_baud(115200);

    /* Enable FIFO, 8-bit words */
    UART0_LCRH_R = UART_LCRH_FEN | UART_LCRH_WLEN_8;

    /* Use system clock */
    UART0_CC_R = UART_CC_CS_SYSCLK;

    /* Enable UART */
    UART0_CTL_R |= UART_CTL_UARTEN | UART_CTL_RXE | UART_CTL_TXE;

    usart_ready = 1;

    return 0;
}
CORE_INITIALIZER(init_usart)

/* Sets baud rate registers */
void usart_baud(uint32_t baud) {
    float brd = ROM_SysCtlClockGet() / (16 * baud);
    int brdi = (int) brd;
    int brdf = (int) (brd * 64 + 0.5);

    UART0_IBRD_R = brdi;
    UART0_FBRD_R = brdf;
}

int usart_putc(char c, void *env) {
    /* Wait until transmit FIFO not full*/
    while (UART0_FR_R & UART_FR_TXFF) {
        yield_if_possible();
    }

    UART0_DR_R = c;

    return 1;
}

int usart_puts(char *s, void *env) {
    int total = 0;
    while (*s) {
        int ret = usart_putc(*s++, env);
        if (ret > 0) {
            total += ret;
        }
        else {
            total = ret;
            break;
        }
    }

    return total;
}

char usart_getc(void *env, int *error) {
    if (error != NULL) {
        *error = 0;
    }

    /* Wait for data */
    while (UART0_FR_R & UART_FR_RXFE) {
        yield_if_possible();
    }

    return UART0_DR_R & UART_DR_DATA_M;
}

int usart_close(resource *resource) {
    printk("OOPS: USART is a fundamental resource, it may not be closed.");
    return -1;
}

static struct mutex driver_mutex = INIT_MUTEX;

static int lm4f_usart_probe(const char *name) {
    /* Statically built driver always exists */
    return 1;
}

static struct obj *lm4f_usart_ctor(const char *name) {
    struct char_device *dev;

    dev = resource_to_char_device(&usart_resource);
    if (!dev) {
        return NULL;
    }

    return &dev->obj;
}

static int lm4f_usart_register(void) {
    struct device_driver *new = kmalloc(sizeof(*new));
    if (!new) {
        fprintf(stderr, "%s: Unable to allocate device driver", __func__);
        return -1;
    }

    new->name = "lm4f-static-usart";
    new->probe = lm4f_usart_probe;
    new->ctor = lm4f_usart_ctor;
    new->class = NULL;
    new->mut = &driver_mutex;

    device_driver_register(new);

    return 0;
}
CORE_INITIALIZER(lm4f_usart_register)
