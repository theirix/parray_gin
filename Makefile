EXTENSION    = parray_gin
EXTVERSION   = 1.2.0
MODULE_big   = $(EXTENSION)
OBJS         = $(patsubst %.c,%.o,$(wildcard src/*.c))
DOCS         = $(wildcard doc/*.md)
TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS += --inputdir=test
PG_CONFIG    := pg_config
#PG_CPPFLAGS  = -g -O0
EXTRA_CLEAN  = sql/$(EXTENSION)--$(EXTVERSION).sql

all: sql/$(EXTENSION)--$(EXTVERSION).sql

sql/$(EXTENSION)--$(EXTVERSION).sql: sql/$(EXTENSION).sql
	cp $< $@

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
