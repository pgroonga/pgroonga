PACKAGES =

MODULE_big = pgroonga_standby_maintainer
include makefiles/pgroonga-standby-maintainer-sources.mk
include makefiles/pgroonga-headers.mk
OBJS = $(SRCS:.c=.o)
$(OBJS): $(HEADERS)

include makefiles/pgrn-pgxs.mk
