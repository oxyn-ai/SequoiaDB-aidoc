EXTENSION = documentdb_docsql
MODULE_big = pg_$(EXTENSION)

SQL_DEPDIR=.deps/sql
SQL_BUILDDIR=build/sql

template_sql_files = $(wildcard sql/*.sql)
generated_sql_files = $(patsubst %,build/%,$(template_sql_files))
DATA_built = $(generated_sql_files)

BUILD_SCRIPT_DIR = ../
OSS_SRC_DIR = ../
OSS_COMMON_SQL_HEADER = $(wildcard $(OSS_SRC_DIR)/common_header.sql)

# Extension defines
API_SCHEMA_NAME=documentdb_docsql
API_SCHEMA_NAME_V2=documentdb_docsql
API_SCHEMA_INTERNAL_NAME=documentdb_docsql_internal
API_SCHEMA_INTERNAL_NAME_V2=documentdb_docsql_internal
API_CATALOG_SCHEMA_NAME=documentdb_docsql_catalog
API_CATALOG_SCHEMA_NAME_V2=documentdb_docsql_catalog
CORE_SCHEMA_NAME=documentdb_core
API_DATA_SCHEMA_NAME=documentdb_docsql_data
API_ADMIN_ROLE=documentdb_docsql_admin_role
API_READONLY_ROLE=documentdb_docsql_readonly_role
POSTGIS_SCHEMA_NAME=public
EXTENSION_OBJECT_PREFIX=documentdb_docsql
API_GUC_PREFIX=documentdb_docsql

USE_DOCUMENTDB_CORE = 1
include $(OSS_SRC_DIR)/Makefile.cflags
SOURCES = $(wildcard src/*.c) $(wildcard src/**/*.c)

OBJS = $(patsubst %.c,%.o,$(SOURCES))

DEBUG ?= no
ifeq ($(DEBUG),yes)
  PG_CPPFLAGS += -ggdb -O0 -g
  PG_CFLAGS += -ggdb -O0 -g
endif

SHLIB_LINK = $(libpq)

include $(OSS_SRC_DIR)/Makefile.global

clean-sql:
	rm -rf .deps/ build/

check:
	$(MAKE) -C src/test all

check-regress:
	$(MAKE) -C src/test check-regress

install: trim_installed_data_files

trim_installed_data_files:
	rm -f $(DESTDIR)$(datadir)/$(datamoduledir)/$(EXTENSION)--*.sql

build-sql: $(generated_sql_files)

$(generated_sql_files): build/%: %
	@mkdir -p $(SQL_DEPDIR) $(SQL_BUILDDIR)
	@# -MF is used to store dependency files(.Po) in another directory for separation
	@# -MT is used to change the target of the rule emitted by dependency generation.
	@# -P is used to inhibit generation of linemarkers in the output from the preprocessor.
	@# -undef is used to not predefine any system-specific or GCC-specific macros.
	@# -imacros is used to specify a file that defines macros for the global context but its output is thrown away.
	@# `man cpp` for further information
	cpp -undef -w $(SQL_DEFINES) -imacros $(OSS_COMMON_SQL_HEADER) -P -MMD -MP -MF$(SQL_DEPDIR)/$(*F).Po -MT$@ $< > $@

include $(OSS_SRC_DIR)/Makefile.versions
