MODULE_big = pgroonga
SRCS = pgroonga.c pgroonga_types.c
OBJS = $(SRCS:.c=.o)
EXTENSION = pgroonga
EXTENSION_VERSION =						\
	$(shell grep default_version $(EXTENSION).control |	\
		sed -e "s/^.*'\([0-9.]*\)'$$/\1/")
DATA = pgroonga--$(EXTENSION_VERSION).sql
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

all: pgroonga--$(EXTENSION_VERSION).sql
pgroonga--$(EXTENSION_VERSION).sql: pgroonga.sql
	@cp $< $@

installcheck: results/full-text-search/text/single/contain
installcheck: results/full-text-search/text/single/match
installcheck: results/full-text-search/text/single/and
installcheck: results/full-text-search/text/multiple/contain
installcheck: results/full-text-search/text/options/tokenizer
installcheck: results/full-text-search/text/options/normalizer
installcheck: results/compare/text/single/equal

results/full-text-search/text/single/contain:
	@mkdir -p $@
results/full-text-search/text/single/match:
	@mkdir -p $@
results/full-text-search/text/single/and:
	@mkdir -p $@
results/full-text-search/text/multiple/contain:
	@mkdir -p $@
results/full-text-search/text/options/tokenizer:
	@mkdir -p $@
results/full-text-search/text/options/normalizer:
	@mkdir -p $@
results/compare/text/single/equal:
	@mkdir -p $@
