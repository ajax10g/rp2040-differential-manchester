To generate the .uf2 binaries:
For pico board:
mkdir build; cd ./build
cmake -DPICO_BOARD=pico ..
make -j$(nproc)
