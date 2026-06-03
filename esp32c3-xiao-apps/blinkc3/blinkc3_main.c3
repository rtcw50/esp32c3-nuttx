
module blinkc3_main;
import std::io;

extern fn int open(ZString path, int oflags);
extern fn int ioctl(int fd, int request, int arg);
extern fn int close(int fd);
extern fn int usleep(int usec);
extern fn int system_O_RDWR();
extern fn int system_GPIO_OUTPUT_PIN();
extern fn int system_GPIOC_SETPINTYPE();
extern fn int system_GPIOC_WRITE();
extern fn int system_GPIOC_READ();

const int BLUE_LED = 2; // index 0 of /dev/gpio0 -> D3
const int RED_LED = 3;

faultdef OPEN_ERROR;

fn int? c3open(ZString path)  {
   int fd = open(path, system_O_RDWR());
   if (fd < 0) {
      return OPEN_ERROR~; 
   }
   return fd;
}

fn int blinkc3_main() @export("blinkc3_main") {
   io::printn("blinkc3 program");
   int? fd = c3open("/dev/gpio0");
   if (catch excuse = fd) { 
      io::printn("failed to open /dev/gpio0");
      return -1;
   }
   io::printn("opened /dev/gpio0");
   defer {
      // close the file descriptor
      close(fd);
   }
      // set direction to output
   if (ioctl(fd, system_GPIOC_SETPINTYPE(), system_GPIO_OUTPUT_PIN()) < 0) {
      io::printn("ioctl failed");
      return -1;
   }
   // blink the blue LED
   for (int i = 0; i < 10; i++) {
      if (ioctl(fd, system_GPIOC_WRITE(), 1) < 0) {
         io::printn("ioctl failed");
         return -1;
      }
      io::printn("blue LED on");
      usleep(500000);
      if (ioctl(fd, system_GPIOC_WRITE(), 0) < 0) {
         io::printn("ioctl failed");
         return -1;
      }
      io::printn("blue LED off");
      usleep(500000);
   }
   return 0;
}
