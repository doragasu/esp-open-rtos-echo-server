PRINTF_SCANF_FLOAT_SUPPORT=0
# Increase number of simultaneous sockets to 5
EXTRA_C_CXX_FLAGS=-DMEMP_NUM_NETCONN=10
ESPTOOL=python2 $(HOME)/src/esp8266/esptool/esptool.py
