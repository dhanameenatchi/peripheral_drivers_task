# Zephyr side running commands

cd ~/zephyrproject/zephyr_hce_task/hce_drivers/zephyr_app

rm -rf build_uart

west build \
-b nucleo_f446re \
--build-dir build_uart \
-p always \
-- \
-DCONF_FILE=config/prj.conf

west flash --build-dir build_uart

ls /dev/ttyACM*

minicom -D /dev/ttyACM0 -b 921600