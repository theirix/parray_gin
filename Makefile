MODULE_big = json_accessors
OBJS = json_accessors.o cJSON.o

EXTENSION = json_accessors_c
DATA = json_accessors_c--1.2.sql

PG_CPPFLAGS = -g -O0

PG_CONFIG := pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
