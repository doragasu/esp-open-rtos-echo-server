PROGRAM=tcp-echo
include $(HOME)/src/esp8266/esp-open-rtos/common.mk

MDMAP ?= /home/jalon/src/github/mdmap/mdmap -w

.PHONY: cart boot blank_cfg blank_wificfg
cart: firmware/$(PROGRAM).bin
	@$(MDMAP) $<:0x2000

boot:
	@$(MDMAP) $(RBOOT_BIN)
#	@$(MDMAP) $(RBOOT_PREBUILT_BIN)

blank_cfg:
	@$(MDMAP) $(RBOOT_CONF):0x1000

