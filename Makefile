MODULES = postgre-json-functions
OBJS = postgre-json-functions.o

EXTENSION = postgre-json-functions
DATA = postgre-json-functions--1.0.sql 

PG_CONFIG := pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
