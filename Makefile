KVER ?= $(shell uname -r)
SRC_DIR := /lib/modules/$(KVER)/build

# TODO:(#REFACTOR)
# 	/map_profiles and /compression_profiles should have their own Makefiles

all: build

build:
		$(MAKE) -j -C $(SRC_DIR) M=$(PWD) modules
clean:
		$(MAKE) -j -C $(SRC_DIR) M=$(PWD) clean