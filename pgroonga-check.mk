PACKAGES =

MODULE_big = pgroonga_check
include makefiles/pgroonga-check-sources.mk
include makefiles/pgroonga-headers.mk
OBJS = $(SRCS:.c=.o)
$(OBJS): $(HEADERS)

include makefiles/pgrn-pgxs.mk

