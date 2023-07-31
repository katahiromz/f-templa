cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .
ninja
strip build\f-templa.exe
strip build\templa.exe
strip build\wildcard.exe
