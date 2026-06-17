#! /bin/bash

pushd apps 
git diff -p > ../apps.patch
popd


