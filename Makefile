MODULE_big = pgroonga
SRCS = pgroonga.c pgroonga_types.c
OBJS = $(SRCS:.c=.o)
EXTENSION = pgroonga
DATA = pgroonga--0.2.0.sql
PG_CPPFLAGS = $(shell pkg-config --cflags groonga)
SHLIB_LINK = $(shell pkg-config --libs groonga)
# REGRESS = pgroonga update bench
REGRESS = $(shell find sql -name '*.sql' | sed -e 's,\(^sql/\|\.sql$$\),,g')
REGRESS_OPTS = --load-extension=pgroonga

ifdef DEBUG
COPT += -O0 -g3 -DPGROONGA_DEBUG=1
SHLIB_LINK += -Wl,--rpath=$(shell pkg-config --libs-only-L groonga | sed -e 's/^-L//')
endif

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

installcheck: results/text/single/contain
installcheck: results/text/multiple/contain

results/text/single/contain:
	@mkdir -p results/text/single/contain
results/text/multiple/contain:
	@mkdir -p results/text/multiple/contain
