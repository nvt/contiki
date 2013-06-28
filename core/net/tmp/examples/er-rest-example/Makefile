all: er-example-server er-example-client
# use this target explicitly if requried: er-plugtest-server


CONTIKI=../..

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

RESOURCES_DIR = ./resources

PROJECTDIRS += $(RESOURCES_DIR)
PROJECT_SOURCEFILES += $(notdir $(shell find $(RESOURCES_DIR) -name '*.c'))

# linker optimizations
SMALL=1

# REST Engine shall use Erbium CoAP implementation
APPS += erbium
APPS += rest-engine

# optional rules to get assembly
#CUSTOM_RULE_C_TO_OBJECTDIR_O = 1
#CUSTOM_RULE_S_TO_OBJECTDIR_O = 1

include $(CONTIKI)/Makefile.include

ifeq ($(TARGET), native)
CFLAGS += -DHARD_CODED_ADDRESS=\"fdfd::10\"
${info INFO: er-example compiling with large buffers}
CFLAGS += -DREST_MAX_CHUNK_SIZE=1024
CFLAGS += -DCOAP_MAX_HEADER_SIZE=176
endif

# optional rules to get assembly
#$(OBJECTDIR)/%.o: asmdir/%.S
#	$(CC) $(CFLAGS) -MMD -c $< -o $@
#	@$(FINALIZE_DEPENDENCY)
#
#asmdir/%.S: %.c
#	$(CC) $(CFLAGS) -MMD -S $< -o $@

# border router rules
$(CONTIKI)/tools/tunslip6:	$(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 aaaa::1/64

connect-router-cooja:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -a 127.0.0.1 aaaa::1/64
