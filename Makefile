MODULE_big = postgre-json-functions
OBJS = postgre-json-functions.o cJSON.o

EXTENSION = postgre-json-functions
DATA = postgre-json-functions--1.0.sql 

REGRESS = postgre-json-functions postgre-json-gin
REGRESS_OPTS = --user postgres

PG_CPPFLAGS = -g -O0

PG_CONFIG := pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
