all:
	$(MAKE) -f pgroonga.mk all
	$(MAKE) -f pgroonga-check.mk all
	$(MAKE) -f pgroonga-database.mk all

clean:
	$(MAKE) -f pgroonga.mk clean
	$(MAKE) -f pgroonga-check.mk clean
	$(MAKE) -f pgroonga-database.mk clean

install:
	$(MAKE) -f pgroonga.mk install
	$(MAKE) -f pgroonga-check.mk install
	$(MAKE) -f pgroonga-database.mk install

installcheck:
	$(MAKE) -f pgroonga.mk installcheck
	#$(MAKE) -f pgroonga-check.mk installcheck
	#$(MAKE) -f pgroonga-database.mk installcheck
