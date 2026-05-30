
module helloc3_main;
import std::io;
//extern fn void puts(char *);
//const char *HELLO = "Hello World\0";

fn int helloc3_main() @export("helloc3_main") {
     io::printn("Hello, World!");
     io::printn("New string to print");
        
//   puts("Hello World");
//   puts(HELLO);
   return 0;
}
