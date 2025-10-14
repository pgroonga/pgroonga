REQUIRED_GROONGA_VERSION = 15.1.8
GROONGA_PKG = "groonga >= $(REQUIRED_GROONGA_VERSION)"
PACKAGES += $(GROONGA_PKG)

PGRN_VERSION =						\
	$(shell grep default_version pgroonga.control |	\
		sed -e "s/^.*'\([0-9.]*\)'$$/\1/")

PG_CPPFLAGS += $(shell pkg-config --cflags $(PACKAGES))
SHLIB_LINK += $(shell pkg-config --libs $(PACKAGES)) -lm

PG_CPPFLAGS += -DPGRN_VERSION="\"${PGRN_VERSION}\""
ifdef PGRN_DEBUG
PG_CPPFLAGS += -O0 -g3 -DPGROONGA_DEBUG=1
ifeq ($(uname), Linux)
SHLIB_LINK += -Wl,--rpath=$(shell pkg-config --libs-only-L $(PACKAGES) | sed -e 's/^-L//')
endif
endif

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
