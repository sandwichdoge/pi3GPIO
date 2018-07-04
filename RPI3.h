#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>


//BCM2837 Broadcom ARM processor GPIO peripheral access library
//For Pi2 v1.2 to Pi3+ (https://www.raspberrypi.org/documentation/hardware/raspberrypi/)
//sandwichdoge@gmail.com


/*See datasheet page 6 for this part.
It is explicitly stated that all access must be done through physical address (0x3F000000)
0x200000 is the offset of GPIO peripheral from peripheral base address in memory*/
#define BCM_PERI_BASE 0x3F000000
#define GPIO_BASE (BCM_PERI_BASE + 0x200000)  
#define BLOCK_SZ (4*1024)


typedef struct peri_t {
    unsigned long devmem_fd;  //file descriptor of /dev/mem
    void *map;  //address of mapped memory
    volatile unsigned *addr;  //formatted address of mapped memory
} peri_t;


peri_t GPIO;
extern peri_t GPIO;


#define SET_IN(p) *(GPIO.addr + ((p)/10)) &= ~(7<<((p%10)*3))
/*
Function    : set pin 'p' to INPUT mode
p           : GPIO pin number
GPIO.addr   : mapped (virtual) base address of GPIO in /dev/mem

Each GPIO pin's Function Select represented by 3 bits (datasheet page 90-92)
we need to flip said bits to 000 to change its function to INPUT mode

GPIO.addr + p/10        : address of register segment that contains pin 'p' in mapped memory
                          one segment is 4bytes long and represents for 10 pins (0-9, 10-19,..)
                          each pin is represented by 3 bits, bit 30-31 is reserved
                          e.g. address of the segment that contains pin 12 is GPIO.addr+1
                          this is a pointer of type int so it's actually 4bytes increment
                          GPIO.addr+1 == 0x3f200000 + 0x4 == 0x3f200004
                          0x3f200004 is the start of the segment that pin 12 belongs in

(p%10)*3                : the position of 3 bits that represent pin p%10 in segment p/10
7 in binary is 111, so ~7 (its complement) is 000, therefore:
7<<((p%10)*3)           : set the bits at (p%10)*3 to 1, therefore:
~(7<<((p%10)*3))        : set the bits at (p%10)*3 to 0
p/10 &= ~(7<<((p%10)*3)): set the 3 bits that represent pin p%10 in segment p/10 to 0

*/


#define SET_OUT(p) *(GPIO.addr + ((p)/10)) |= (1<<((p%10)*3))
/*
Function    : set pin number 'p' to OUTPUT mode
Important   : SET_IN must be called first to set all bits to 0
p           : GPIO pin number
GPIO.addr   : mapped (virtual) base address of GPIO in /dev/mem

GPIO.addr + p/10        : see SET_IN()
(p%10)*3                : the position of 3 bits that represent pin p%10 in segment p/10
1<<((p%10)*3)           : set first bit of the 3 FSEL bits to 1, thus changing its function to OUTPUT
                        : SET_OUT() needs to be called first to ensure the other 2 bits are 00
p/10 |= 1<<((p%10)*3) : set the 3 bits that represent pin p%10 in segment p/10 to 001

*/


#define SET_ALT(p, i) *(GPIO.addr + (p/10)) |= ((i<=3?(i+4):(i==4?3:2))<<((p%10)*3))
/*
Function    : set pin number 'p' to ALT mode, for alt functions see datasheet page 102
p           : GPIO pin number
i           : ALT function number
GPIO.addr   : mapped (virtual) base address of GPIO in /dev/mem

GPIO.addr + p/10        : see SET_IN()
For explanation of the function see datasheet page 92-94

*/


#define GPIO_SET *(GPIO.addr + 7)
/*
GPIOSET is 7*4 bytes away from GPIO base address
it has 2 segments, each 4bytes long, they can be considered a single 8bytes segment
each bit represents 1 pin, flipping it to 1 is equivalent to writing 1 to said pin
last 10bits are reserved
WRITING "0" HAS NO EFFECT, USE GPIO_CLR INSTEAD!

e.g. GPIO_SET = 1<<p;
this will write 1 to GPIO pin number 'p'
*/

#define GPIO_CLR *(GPIO.addr + 10)
/*
GPIOCLR is 10*4 bytes away from GPIO base address
it has 2 segments, each 4bytes long, they can be considered a single 8bytes segment
each bit represents 1 pin, flipping it to 1 is equivalent to writing 0 to said pin
if the pin is in INPUT mode, this has no effect
last 10bits are reserved
writing "0" has no effect.

e.g. GPIO_CLR = 1<<p;
this will write 0 to GPIO pin number 'p'
*/


#define GPIO_READ(p) (*(GPIO.addr + 13) & (1<<(p)))
/*
GPIOLEV is 13*4 bytes away from GPIO base address
it has 2 segments, each 4bytes long, they can be considered a single 8bytes segment
return 1 if high, 0 if low (0V)

e.g. val = GPIO_READ(p);
this will return status of pin number 'p'
*/


/*Initialize GPIO, always call this before proceeding with any GPIO modifications*/
int map_gpio(peri_t *pGPIO)
{
    pGPIO->devmem_fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (pGPIO->devmem_fd < 0) {
        printf("failed to open /dev/mem\n");
        return -1;
    }
    pGPIO->map = mmap(NULL, BLOCK_SZ, PROT_READ|PROT_WRITE, MAP_SHARED, pGPIO->devmem_fd, GPIO_BASE);
    if (pGPIO->map == MAP_FAILED) {
        printf("failed to map error code: %d\n", errno);
        return -1;
    }
    close(pGPIO->devmem_fd);
    pGPIO->addr = (volatile unsigned*)pGPIO->map;
    return 0;
}


int unmap_gpio(peri_t *pGPIO)
{
    munmap(pGPIO->map, BLOCK_SZ);
    return 0;
}