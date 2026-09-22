#! /bin/bash

pushd apps 
git diff -p > ../apps.patch
popd

mkdir -p /tmp/lvgl
cp apps/graphics/lvgl/v9.2.1.zip /tmp/lvgl
pushd /tmp/lvgl
unzip -o v9.2.1.zip
popd
diff -r --exclude=*.o --text  apps/graphics/lvgl/lvgl  /tmp/lvgl/lvgl-9.2.1 >> apps.patch
rm -rf /tmp/lvgl


