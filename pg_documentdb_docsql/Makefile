EXTENSION = pg_documentdb_docsql
MODULE_big = pg_documentdb_docsql
OBJS = src/docsql.o

DATA = sql/pg_documentdb_docsql--1.0.sql
REGRESS = docsql_basic docsql_crud

PGFILEDESC = "pg_documentdb_docsql - Pure SQL interface for DocumentDB"

EXTENSION_VERSION = 1.0
EXTVERSION = $(EXTENSION_VERSION)

SHLIB_LINK +=

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
