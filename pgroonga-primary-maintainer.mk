PACKAGES =

MODULE_big = pgroonga_primary_maintainer
include makefiles/pgroonga-primary-maintainer-sources.mk
include makefiles/pgroonga-headers.mk
OBJS = $(SRCS:.c=.o)
$(OBJS): $(HEADERS)

include makefiles/pgrn-pgxs.mk
