#! /bin/bash

pushd nuttx
git diff -p > ../nuttx.patch
# New files
diff -uN /dev/null tools/Zenc.defs >> ../nuttx.patch
diff -uN /dev/null tools/C3.defs >> ../nuttx.patch
diff -uN /dev/null boards/risc-v/esp32c3/esp32c3-xiao/src/esp32c3_lcd.c >> ../nuttx.patch
diff -uN /dev/null boards/risc-v/esp32c3/esp32c3-xiao/src/esp32c3_xiao_spi.c >> ../nuttx.patch
diff -uN /dev/null boards/risc-v/esp32c3/esp32c3-xiao/src/esp32c3_tsc.c >> ../nuttx.patch
popd


