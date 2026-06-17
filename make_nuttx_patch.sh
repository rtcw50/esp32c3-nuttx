#! /bin/bash

pushd nuttx
git diff -p > ../nuttx.patch
# New files
diff -uN /dev/null tools/Zenc.defs >> ../nuttx.patch
diff -uN /dev/null tools/C3.defs >> ../nuttx.patch
popd


