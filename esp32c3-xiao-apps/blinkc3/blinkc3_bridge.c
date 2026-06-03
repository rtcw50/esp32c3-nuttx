#include <fcntl.h>
#include <nuttx/ioexpander/gpio.h>

int system_O_RDWR(void) {
   return O_RDWR;
}
int system_GPIO_OUTPUT_PIN(void) {
   return GPIO_OUTPUT_PIN;
}
int  system_GPIOC_SETPINTYPE(void) {
   return GPIOC_SETPINTYPE;
}
int system_GPIOC_WRITE(void) {
   return GPIOC_WRITE;
}
int system_GPIOC_READ(void) {
   return GPIOC_READ;
}