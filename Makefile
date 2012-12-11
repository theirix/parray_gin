EXTENSION    = parray_gin
EXTVERSION   = 1.1.3
MODULE_big   = $(EXTENSION)
OBJS         = $(patsubst %.c,%.o,$(wildcard src/*.c))
DATA         = $(wildcard sql/*--*.sql) sql/$(EXTENSION)--$(EXTVERSION).sql
DOCS         = $(wildcard doc/*.md)
TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS += --inputdir=test
PG_CONFIG    := pg_config
PG_CPPFLAGS  = -g -O0
EXTRA_CLEAN  = sql/$(EXTENSION)--$(EXTVERSION).sql

all: sql/$(EXTENSION)--$(EXTVERSION).sql

sql/$(EXTENSION)--$(EXTVERSION).sql: sql/$(EXTENSION).sql
	cp $< $@

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
