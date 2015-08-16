MODULE_big = pgroonga
SRCS = pgroonga.c
OBJS = $(SRCS:.c=.o)
EXTENSION = pgroonga
EXTENSION_VERSION =						\
	$(shell grep default_version $(EXTENSION).control |	\
		sed -e "s/^.*'\([0-9.]*\)'$$/\1/")
DATA = pgroonga--$(EXTENSION_VERSION).sql
PG_CPPFLAGS = $(shell pkg-config --cflags groonga)
SHLIB_LINK = $(shell pkg-config --libs groonga) -lm
REGRESS = $(shell find sql -name '*.sql' | sed -e 's,\(^sql/\|\.sql$$\),,g')
REGRESS_OPTS = --load-extension=pgroonga

ifdef DEBUG
COPT += -O0 -g3 -DPGROONGA_DEBUG=1
SHLIB_LINK += -Wl,--rpath=$(shell pkg-config --libs-only-L groonga | sed -e 's/^-L//')
endif

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

all: pgroonga--$(EXTENSION_VERSION).sql
pgroonga--$(EXTENSION_VERSION).sql: pgroonga.sql
	@cp $< $@

RESULT_DIRS = $(shell find sql -type d | sed -e 's,^sql/,results/,')

installcheck: $(RESULT_DIRS)

$(RESULT_DIRS):
	@mkdir -p $@
