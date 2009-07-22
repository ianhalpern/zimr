CC          = gcc -Wall
CFLAGS      = -c -fPIC
LDFLAGS     =
DBFLAGS     = -ggdb -O0 -pg
TARGET_ARCH =
PTHREAD     = -pthread
DSYMBOLS    = -DBUILD_DATE="\"`date`\"" -DPODORA_VERSION=\"$(VERNUM)\"
OUTPUT      = -o $@
SHARED      = -shared -fPIC -Wl,-soname,$@
LDPODORA    = -lpodora -L. -Wl,-rpath,`pwd`

OBJS        = general.o pcom.o website.o pfildes.o request.o response.o mime.o
EXEC_OBJS   = podora.o podora-website.o pcom-test-client.o pcom-test-server.o

EXECS       = podora podora-website
TEST_EXECS  =
SHARED_OBJS = libpodora.so

SRCDIR     = src
LIB_SRCDIR = $(SRCDIR)/lib

VERNUM  = `vernum`

OBJ_DEPENDS        = %.o: $(SRCDIR)/%.c $(SRCDIR)/%.h $(SRCDIR)/config.h
EXEC_DEPENDS       = %: $(SRCDIR)/%/main.c $(SRCDIR)/config.h
SHARED_OBJ_DEPENDS = lib%.so: $(LIB_SRCDIR)/%.c $(LIB_SRCDIR)/%.h $(SRCDIR)/config.h

EXEC_COMPILE = $(CC) $(LDFLAGS) $(TARGET_ARCH) $(DSYMBOLS) -I$(SRCDIR) -I$(LIB_SRCDIR) $(OUTPUT) $^
OBJ_COMPILE  = $(CC) $(CFLAGS) $(TARGET_ARCH) $(OUTPUT) $<

##### USER BUILD COMMANDS #####
###############################

make debug beta: $(SHARED_OBJS) $(EXECS)

tests: $(TEST_EXECS)

clean:
	rm -f $(EXECS) $(TEST_EXECS) *.o *.so gmon.out

##### EXECS #####
#################

podora: $(EXEC_DEPENDS) general.o pcom.o pfildes.o website.o
	$(EXEC_COMPILE)

podora-website: $(EXEC_DEPENDS)
	$(EXEC_COMPILE) $(LDPODORA)

##### SHARED OBJS #####

libpodora.so: $(SHARED_OBJ_DEPENDS) general.o pcom.o pfildes.o website.o mime.o request.o response.o
	$(EXEC_COMPILE) $(SHARED)

##### OBJS ######
#################

$(OBJS): $(OBJ_DEPENDS)
	$(OBJ_COMPILE)

##### COMMAND VARIABLE MODS #####
#################################

debug: CC += $(DBFLAGS)
tests: CC += $(DBFLAGS)
$(TEST_EXECS): CC += $(DBFLAGS)
debug: VERNUM = `vernum 1`b-debug
beta:  VERNUM = `vernum 1`b
