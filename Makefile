CC = cc

cflags = -Wall -O2
ldflags =

all_incs = `pkg-config --cflags glib-2.0 gtk+-3.0 libpulse libpulse-mainloop-glib`
all_libs = `pkg-config --libs   glib-2.0 gtk+-3.0 libpulse libpulse-mainloop-glib`

all_objects = src/paxui.o \
			  src/paxui-pulse.o \
			  src/paxui-gui.o \
			  src/paxui-actions.o \
			  src/paxui-data.o

module_hdrs = src/paxui.h \
			  src/paxui-pulse.h \
			  src/paxui-gui.h \
			  src/paxui-actions.h \
			  src/paxui-data.h

all_icons = data/logo.png \
			data/source.png \
			data/source-dark.png \
			data/sink.png \
			data/sink-dark.png \
			data/gear.png \
			data/gear-dark.png \
			data/muted.png \
			data/muted-dark.png \
			data/locked.png \
			data/locked-dark.png \
			data/unlocked.png \
			data/unlocked-dark.png \
			data/switch.png \
			data/switch-dark.png


all:	paxui

paxui:	$(all_objects)
	@echo LINK
	@$(CC) -o paxui $(all_objects) $(ldflags) $(all_libs)

clean:
	@echo CLEAN
	@rm -f src/*.o src/paxui-data.c src/paxui-data.h paxui

src/paxui-data.h: src/paxui-data.xml $(all_icons) data/internal.css
	@echo $@
	@glib-compile-resources --sourcedir=data --target=src/paxui-data.h --generate-header src/paxui-data.xml

src/paxui-data.c: src/paxui-data.xml $(all_icons) data/internal.css
	@echo $@
	@glib-compile-resources --sourcedir=data --generate-source src/paxui-data.xml

$(all_objects): %.o: %.c %.h $(module_hdrs)
	@echo $@
	@$(CC) -c $(cflags) $(all_incs) -o $@ $<
