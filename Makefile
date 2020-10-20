CC = cc

cflags = -Wall -O2
ldflags =

all_incs = `pkg-config --cflags glib-2.0 gtk+-3.0 libpulse libpulse-mainloop-glib`
all_libs = `pkg-config --libs   glib-2.0 gtk+-3.0 libpulse libpulse-mainloop-glib`

all_objects = src/paxui.o \
			  src/paxui-pulse.o \
			  src/paxui-gui.o \
			  src/paxui-actions.o \
			  src/paxui-icons.o

module_hdrs = src/paxui.h \
			  src/paxui-pulse.h \
			  src/paxui-gui.h \
			  src/paxui-actions.h \
			  src/paxui-icons.h

all_icons = png/logo.png \
			png/source.png \
			png/sink.png \
			png/view.png \
			png/source-dark.png \
			png/sink-dark.png \
			png/view-dark.png


all:	paxui

paxui:	$(all_objects)
	@echo LINK
	@$(CC) -o paxui $(all_objects) $(ldflags) $(all_libs)

clean:
	@echo CLEAN
	@rm -f src/*.o src/paxui-icons.c src/paxui-icons.h paxui

src/paxui-icons.h: src/paxui-icons.xml $(all_icons)
	@echo $@
	@glib-compile-resources --sourcedir=png --target=src/paxui-icons.h --generate-header src/paxui-icons.xml

src/paxui-icons.c: src/paxui-icons.xml $(all_icons)
	@echo $@
	@glib-compile-resources --sourcedir=png --generate-source src/paxui-icons.xml

$(all_objects): %.o: %.c %.h $(module_hdrs)
	@echo $@
	@$(CC) -c $(cflags) $(all_incs) -o $@ $<
