CC          = gcc -Wall
CFLAGS      = -c -fPIC
LDFLAGS     =
DBFLAGS     = -ggdb -g -O0
PROFILEFLAGS= -pg
TARGET_ARCH =
PTHREAD     = -pthread
DSYMBOLS    = -DZIMR_VERSION="\"$(VERSION)\""
OUTPUT      = -o $@
SHARED      = -shared -fPIC -Wl,-soname,$@
LDPYTHON    = -lpython$(PYVERSION) -I/usr/include/python$(PYVERSION) -lpthread
PYMOD       = -shared -fPIC -Wl,-O1 -Wl,-Bsymbolic-functions $(LDPYTHON)
LDZIMR      = -lzimr -L.
LDZIMR_LOCAL= -Wl,-rpath,`pwd`

PYVERSION = `bash -c "python --version |& sed 's/Python \(.*\)\..*/\1/'"`

OBJS        = general.o website.o zfildes.o zsocket.o connection.o\
			  mime.o headers.o params.o cookies.o urldecoder.o daemon.o zcnf.o\
			  simclist.o msg_switch.o userdir.o cli.o dlog.o md5.o fd_hash.o trace.o

EXECS       = zimrd zimr #zimr-app
TEST_EXECS  = test-zsocket-client test-zsocket-server test-strnstr test-client test-server test-fd-hash
SHARED_OBJS = libzimr.so
PYMOD_OBJS  = czimr.so
MODULE_NAMES= modpython modtest modauth

MODULES := $(addsuffix .so, $(addprefix modules/, $(MODULE_NAMES) ) )


SRCDIR     = src
LIB_SRCDIR = $(SRCDIR)/lib
PY_SRCDIR  = $(SRCDIR)/python
MOD_SRCDIR = $(SRCDIR)/modules

INSTALL_EXECDIR = /usr/bin
INSTALL_LIBDIR  = /usr/lib
INSTALL_PYDIR   = /usr/lib/python$(PYVERSION)
INSTALL_MODDIR  = /usr/lib/zimr
INSTALL_CNFDIR  = /etc/zimr

VERSION    = $$( git describe --tags || cat VERSION )
VERSION_1  = $$( git describe --tags || cat VERSION )

OBJ_DEPENDS        = %.o: $(SRCDIR)/%.c $(SRCDIR)/%.h $(SRCDIR)/config.h
EXEC_DEPENDS       = %: $(SRCDIR)/%/main.c $(SRCDIR)/config.h
TEST_DEPENDS       = test-%: $(SRCDIR)/test/%.c $(SRCDIR)/config.h
SHARED_OBJ_DEPENDS = lib%.so: $(LIB_SRCDIR)/%.c $(LIB_SRCDIR)/%.h $(SRCDIR)/config.h
PYMOD_DEPENDS      = %.so: $(PY_SRCDIR)/%module.c $(SRCDIR)/config.h
MOD_DEPENDS        = modules/mod%.so: $(MOD_SRCDIR)/mod%.c $(SRCDIR)/config.h

EXEC_COMPILE = $(CC) $(LDFLAGS) $(TARGET_ARCH) $(DSYMBOLS) -I$(SRCDIR) -I$(LIB_SRCDIR) $(OUTPUT) $^
OBJ_COMPILE  = $(CC) $(CFLAGS) $(TARGET_ARCH) $(OUTPUT) $<

include $(addsuffix .mk, $(addprefix $(MOD_SRCDIR)/, $(MODULE_NAMES) ) )

##### TOP-LEVEL COMMANDS #####
###############################
.PHONY: make debug profile-debug tests clean install

make debug local-profile-debug profile-debug local-debug: version $(SHARED_OBJS) $(EXECS) $(PYMOD_OBJS) $(MODULES)

version:
	@echo $(VERSION_1) > VERSION

tests: $(TEST_EXECS)

clean:
	rm -f $(EXECS) $(TEST_EXECS) $(MODULES) *.o *.so gmon.out

install:
	@echo "--- Copying zimr execs, libs, and modules ---";
	@mkdir -p $(INSTALL_EXECDIR) $(INSTALL_LIBDIR) $(INSTALL_PYDIR) $(INSTALL_MODDIR) $(INSTALL_CNFDIR)
	cp -u default-configs/* $(INSTALL_CNFDIR)
	cp --remove-destination $(EXECS) $(INSTALL_EXECDIR)
	cp --remove-destination $(SHARED_OBJS) $(INSTALL_LIBDIR)
	cd python && python setup.py $$( cd .. && echo $(VERSION) ) $(PYVERSION) install
	cp --remove-destination -r modules/* $(INSTALL_MODDIR)
	@echo "--- Success ---";
	@echo;
	@echo "--- Setting up system to autostart zimr ---";
	cp init.d/* /etc/init.d/
	@ls init.d | awk '{ x = "chmod 755 /etc/init.d/" $$0; print x; system( x ); }'
	@ls init.d | awk '{ x = "update-rc.d " $$0 " defaults > /dev/null"; print x; system( x ); }'
	@echo "--- Success ---";
	@echo;
	@echo "Finished. Installation succeeded!";

##### EXECS #####
#################

zimr: $(EXEC_DEPENDS) libzimr.so userdir.o cli.o
	$(EXEC_COMPILE) $(LDZIMR) -lproc

zimrd: $(EXEC_DEPENDS) general.o zfildes.o website.o zsocket.o daemon.o msg_switch.o simclist.o zcnf.o fd_hash.o trace.o
	$(EXEC_COMPILE) -lyaml -lssl -lcrypto

##### SHARED OBJS #####

libzimr.so: $(SHARED_OBJ_DEPENDS) general.o msg_switch.o zfildes.o website.o mime.o connection.o\
			 					  headers.o params.o cookies.o zsocket.o urldecoder.o zcnf.o simclist.o userdir.o\
								  dlog.o md5.o fd_hash.o trace.o
	$(EXEC_COMPILE) $(SHARED) -lyaml -lssl -ldl -Wl,-rpath,$(INSTALL_MODDIR)/

##### PYTHON MODS #####

czimr.so: $(PYMOD_DEPENDS) libzimr.so
	$(EXEC_COMPILE) $(PYMOD) $(LDZIMR)
	cp $@ python/zimr/czimr.so

##### MODULES #####

$(MODULES): $(MOD_DEPENDS)
	mkdir -p modules/
	$(EXEC_COMPILE) $(SHARED) $(MODULE_CC_ARGS)

##### TESTS #####

test-strnstr: $(TEST_DEPENDS) general.o
	$(EXEC_COMPILE)

test-client: $(TEST_DEPENDS) msg_switch.o zsocket.o zfildes.o general.o simclist.o
	$(EXEC_COMPILE) -lssl

test-server: $(TEST_DEPENDS) msg_switch.o zsocket.o zfildes.o general.o simclist.o
	$(EXEC_COMPILE) -lssl

test-zsocket-client: $(TEST_DEPENDS) zsocket.o zfildes.o simclist.o general.o
	$(EXEC_COMPILE) -lssl

test-zsocket-server: $(TEST_DEPENDS) zsocket.o zfildes.o general.o simclist.o
	$(EXEC_COMPILE) -lssl

test-fd-hash: $(TEST_DEPENDS) fd_hash.o
	$(EXEC_COMPILE)

##### OBJS ######
#################

$(OBJS): $(OBJ_DEPENDS)
	$(OBJ_COMPILE)

####################

##### COMMAND VARIABLE MODS #####
#################################
$(TEST_EXECS) tests profile-local-debug profile-debug local-debug debug: CC += $(DBFLAGS)
local-profile-debug profile-debug debug local-debug: DSYMBOLS += -DDEBUG
local-profile-debug profile-debug debug local-debug: VERSION := $(VERSION)-debug
local-profile-debug profile-debug: CC += $(PROFILEFLAGS)
local-profile-debug local-debug: LDZIMR += $(LDZIMR_LOCAL)
#debug: EXEC_COMPILE += -rdynamic -ldl $(SRCDIR)/sigsegv.c
