
# Do not edit normally. Configuration settings are in Makefile.conf.

include Makefile.conf

VERSION = 0.1
SO_VERSION = 0.1

ifeq ($(USE_DEBUG_FLAGS), YES)
OPTCFLAGS += -ggdb -O
else
OPTCFLAGS += -Ofast -ffast-math
endif
CPPFLAGS = -std=c++98 -Wall -Wno-maybe-uninitialized -pipe -I. $(OPTCFLAGS)
CPPFLAGS += -DDETEX_COMPRESS_VERSION=\"v$(VERSION)\"

MODULE_OBJECTS = detex-compress.o compress.o png.o mipmaps.o compress-bc1.o \
	compress-bc2-bc3.o compress-rgtc.o compress-etc.o
PROGRAMS = detex-compress

default : detex-compress

install : install_programs

install-programs : detex-compress
	install -m 0755 detex-compress $(PROGRAM_INSTALL_DIR)/detex-compress

detex-compress : $(MODULE_OBJECTS)
	g++ $(MODULE_OBJECTS) -o detex-compress \
	`pkg-config --libs libpng datasetturbo` -ldetex -lm

clean :
	rm -f $(MODULE_OBJECTS)
	rm -f $(PROGRAMS)

.cpp.o :
	g++ -c $(CPPFLAGS) $< -o $@ `pkg-config --cflags datasetturbo`

dep :
	rm -f .depend
	make .depend

.depend: Makefile.conf Makefile
	g++ -MM $(CPPFLAGS) $(patsubst %.o,%.cpp,$(MODULE_OBJECTS)) > .depend
        # Make sure Makefile.conf and Makefile are dependency for all modules.
	for x in $(MODULE_OBJECTS); do \
	echo $$x : Makefile.conf Makefile >> .depend; done

include .depend

