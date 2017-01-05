PACKAGES =

MODULE_big = pgroonga_check
include makefiles/pgroonga-check-sources.mk
OBJS = $(SRCS:.c=.o)

include makefiles/pgrn-pgxs.mk

