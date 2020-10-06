CC = cc

cflags = -Wall -O2
ldflags =

all_incs = `pkg-config --cflags glib-2.0 gtk+-3.0 libpulse libpulse-mainloop-glib`
all_libs = `pkg-config --libs   glib-2.0 gtk+-3.0 libpulse libpulse-mainloop-glib`

all_objects = paxui.o paxui-pulse.o paxui-gui.o paxui-actions.o paxui-icons.o
module_hdrs = paxui.h paxui-pulse.h paxui-gui.h paxui-actions.h paxui-icons.h

all_icons = logo.png \
			source.png sink.png view.png \
			source-dark.png sink-dark.png view-dark.png


all:	paxui

paxui:	$(all_objects)
	@echo LINK
	@$(CC) -o paxui $(all_objects) $(ldflags) $(all_libs)

clean:
	@echo CLEAN
	@rm -f *.o paxui-icons.c paxui-icons.h paxui

paxui-icons.h: paxui-icons.xml $(all_icons)
	@echo $@
	@glib-compile-resources --target=paxui-icons.h --generate-header paxui-icons.xml

paxui-icons.c: paxui-icons.xml $(all_icons)
	@echo $@
	@glib-compile-resources --generate-source paxui-icons.xml

$(all_objects): %.o: %.c %.h $(module_hdrs)
	@echo $@
	@$(CC) -c $(cflags) $(all_incs) -o $@ $<
