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

PYVERSION = 2.6

OBJS        = general.o website.o zfildes.o zsocket.o connection.o\
			  mime.o headers.o params.o cookies.o urldecoder.o daemon.o zcnf.o zerr.o\
			  simclist.o msg_switch.o userdir.o

EXECS       = zimr-proxy zimr-app zimr
TEST_EXECS  = test-strnstr test-client test-server
SHARED_OBJS = libzimr.so
PYMOD_OBJS  = zimr.so
MODULE_NAMES= modpython modtest

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

VERSION  = `command vernum 2> /dev/null || cat VERSION`

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

make debug local-profile-debug profile-debug local-debug: $(SHARED_OBJS) $(EXECS) $(PYMOD_OBJS) $(MODULES)

tests: $(TEST_EXECS)

clean:
	rm -f $(EXECS) $(TEST_EXECS) $(MODULES) *.o *.so gmon.out

install:
	@echo "--- Copying zimr execs, libs, and modules ---";
	@mkdir -p $(INSTALL_EXECDIR) $(INSTALL_LIBDIR) $(INSTALL_PYDIR) $(INSTALL_MODDIR) $(INSTALL_CNFDIR)
	cp -u default-configs/* $(INSTALL_CNFDIR)
	cp --remove-destination $(EXECS) $(INSTALL_EXECDIR)
	cp --remove-destination $(SHARED_OBJS) $(INSTALL_LIBDIR)
	cp --remove-destination -r python/* $(INSTALL_PYDIR)
	cp --remove-destination -r modules/* $(INSTALL_MODDIR)
	@echo "--- Success ---";
	@echo;
	@#@echo "--- Setting up system to autostart zimr ---";
	@#cp init.d/* /etc/init.d/
	@#@ls init.d | awk '{ x = "chmod 755 /etc/init.d/" $$0; print x; system( x ); }'
	@#@ls init.d | awk '{ x = "update-rc.d " $$0 " defaults > /dev/null"; print x; system( x ); }'
	@#@echo "--- Success ---";
	@#@echo;
	@echo "Finished. Installation succeeded!";

##### EXECS #####
#################

zimr: $(EXEC_DEPENDS) zsocket.o zfildes.o zerr.o zcnf.o general.o simclist.o userdir.o
	$(EXEC_COMPILE) -lyaml

zimr-proxy: $(EXEC_DEPENDS) general.o zfildes.o website.o zsocket.o daemon.o msg_switch.o simclist.o zerr.o zcnf.o
	$(EXEC_COMPILE) -lyaml

zimr-app: $(EXEC_DEPENDS) libzimr.so
	$(EXEC_COMPILE) $(LDZIMR)

##### SHARED OBJS #####

libzimr.so: $(SHARED_OBJ_DEPENDS) general.o msg_switch.o zfildes.o website.o mime.o connection.o\
			 					  headers.o params.o cookies.o urldecoder.o zcnf.o zsocket.o zerr.o simclist.o userdir.o
	$(EXEC_COMPILE) $(SHARED) -lyaml -ldl -Wl,-rpath,$(INSTALL_MODDIR)/

##### PYTHON MODS #####

zimr.so: $(PYMOD_DEPENDS) libzimr.so
	$(EXEC_COMPILE) $(PYMOD) $(LDZIMR)
	cp $@ python/zimr/__init__.so

##### MODULES #####

$(MODULES): $(MOD_DEPENDS)
	$(EXEC_COMPILE) $(SHARED) $(MODULE_CC_ARGS)

##### TESTS #####

test-strnstr: $(TEST_DEPENDS) general.o
	$(EXEC_COMPILE)

test-client: $(TEST_DEPENDS) msg_switch.o zsocket.o zerr.o zfildes.o simclist.o
	$(EXEC_COMPILE)

test-server: $(TEST_DEPENDS) msg_switch.o zsocket.o zerr.o zfildes.o general.o simclist.o
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
