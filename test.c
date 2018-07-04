#include "RPI3.h"
#include <time.h>
#include <stdio.h>


//blink led in GPIO18 (BCM port numbering standard)
void main()
{
    map_gpio(&GPIO);

    int gpin = 18;
    SET_IN(gpin);
    SET_OUT(gpin);

    for (int i = 0; i < 10; i++) {
        GPIO_SET = 1 << gpin;
        sleep(1);
        GPIO_CLR = 1 << gpin;
        sleep(1);
    }
    
    unmap_gpio(&GPIO);
    return;
}
