REQUIRED_GROONGA_VERSION = 8.0.2
GROONGA_PKG = "groonga >= $(REQUIRED_GROONGA_VERSION)"
PACKAGES += $(GROONGA_PKG)

PGRN_VERSION =						\
	$(shell grep default_version pgroonga.control |	\
		sed -e "s/^.*'\([0-9.]*\)'$$/\1/")

PG_CPPFLAGS = $(shell pkg-config --cflags $(PACKAGES))
SHLIB_LINK = $(shell pkg-config --libs $(PACKAGES)) -lm

COPT += -DPGRN_VERSION="\"${PGRN_VERSION}\""
ifdef DEBUG
COPT += -O0 -g3 -DPGROONGA_DEBUG=1
SHLIB_LINK += -Wl,--rpath=$(shell pkg-config --libs-only-L $(PACKAGES) | sed -e 's/^-L//')
endif

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
