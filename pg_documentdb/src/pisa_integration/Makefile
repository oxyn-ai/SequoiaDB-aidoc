# PISA Integration Module Makefile

PISA_INTEGRATION_SOURCES = \
	pisa_integration.c \
	data_bridge.c \
	index_sync.c \
	query_router.c \
	documentdb_pisa_export.c

PISA_INTEGRATION_HEADERS = \
	$(top_srcdir)/include/pisa_integration/pisa_integration.h \
	$(top_srcdir)/include/pisa_integration/data_bridge.h \
	$(top_srcdir)/include/pisa_integration/index_sync.h \
	$(top_srcdir)/include/pisa_integration/query_router.h

# Add PISA integration sources to the main build
OBJS += $(PISA_INTEGRATION_SOURCES:.c=.o)

# Include paths for PISA integration headers
CPPFLAGS += -I$(top_srcdir)/include/pisa_integration

# Dependencies
$(PISA_INTEGRATION_SOURCES:.c=.o): $(PISA_INTEGRATION_HEADERS)

# Clean targets
clean-pisa-integration:
	rm -f $(PISA_INTEGRATION_SOURCES:.c=.o)

.PHONY: clean-pisa-integration
