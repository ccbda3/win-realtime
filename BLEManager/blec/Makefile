BTSTACK_ROOT = /home/gps/btstack
PORT_LINUX = $(BTSTACK_ROOT)/port/linux
BUILD_DIR = build

CFLAGS = -g -O2 -Wall \
	-I$(PORT_LINUX) \
	-I$(BTSTACK_ROOT)/src \
	-I$(BTSTACK_ROOT)/src/ble \
	-I$(BTSTACK_ROOT)/src/classic \
	-I$(BTSTACK_ROOT)/src/mesh \
	-I$(BTSTACK_ROOT)/src/le-audio \
	-I$(BTSTACK_ROOT)/platform/posix \
	-I$(BTSTACK_ROOT)/platform/embedded \
	-I$(BTSTACK_ROOT)/platform/linux \
	-I$(BTSTACK_ROOT)/platform/lwip \
	-I$(BTSTACK_ROOT)/platform/lwip/port \
	-I$(BTSTACK_ROOT)/3rd-party/micro-ecc \
	-I$(BTSTACK_ROOT)/3rd-party/rijndael \
	-I$(BTSTACK_ROOT)/3rd-party/tinydir \
	-I$(BTSTACK_ROOT)/3rd-party/md5 \
	-I$(BTSTACK_ROOT)/3rd-party/yxml \
	-I$(BTSTACK_ROOT)/3rd-party/hxcmod-player \
	-I$(BTSTACK_ROOT)/3rd-party/lc3-google/include \
	-I$(BTSTACK_ROOT)/3rd-party/bluedroid/decoder/include \
	-I$(BTSTACK_ROOT)/3rd-party/bluedroid/encoder/include \
	-I$(BTSTACK_ROOT)/3rd-party/lwip/core/src/include \
	-I$(BTSTACK_ROOT)/3rd-party/lwip/dhcp-server \
	-I.

all: ble_daemon blemon

blemon: blemon.c
	gcc -g -O2 -Wall blemon.c -o blemon

$(BUILD_DIR)/libbtstack.a:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake $(PORT_LINUX) > /dev/null 2>&1 && make btstack

ble_gatt.h: mv999.gatt
	python3 $(BTSTACK_ROOT)/tool/compile_gatt.py mv999.gatt ble_gatt.h

ble_daemon: ble_daemon.c ble_gatt.h $(BUILD_DIR)/libbtstack.a
	gcc $(CFLAGS) ble_daemon.c -L$(BUILD_DIR) -lbtstack -lpthread -lm -lz -o ble_daemon

clean:
	rm -f ble_daemon blemon ble_gatt.h
	rm -rf $(BUILD_DIR)

.PHONY: all clean
