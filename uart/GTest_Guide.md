# Host side G Testing and Coverage

cd ~/zephyrproject/zephyr_hce_task/hce_drivers

rm -rf uart_build

mkdir uart_build

cd uart_build

cmake ..

cmake --build . --target uart_test

./uart_test

cmake --build . --target uart_bench

./uart_bench

cd ~/zephyrproject/zephyr_hce_task/hce_drivers/uart_build

make coverage