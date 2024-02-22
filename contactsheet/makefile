CC = gcc
CFLAGS = -g -I/usr/include/gimp-2.0 -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include -I/usr/include/sysprof-4 -I/usr/include/libpng16 -I/usr/include/libmount -I/usr/include/blkid -I/usr/include/cairo -I/usr/include/freetype2 -I/usr/include/harfbuzz -I/usr/include/libxml2 -I/usr/include/pixman-1 -I/usr/include/gegl-0.4 -I/usr/include/gio-unix-2.0 -I/usr/include/glib-1.0 -I/usr/include/babl-0.1 -I/usr/include/gtk-2.0 -I/usr/lib64/gtk-2.0/include -I/usr/include/pango-1.0 -I/usr/include/fribidi -I/usr/include/atk-1.0 -I/usr/include/gexiv2

LIBS = -lgegl-0.4 -lgegl-npd-0.4 -lgimpui-2.0 -lgimpwidgets-2.0 -lgimpmodule-2.0 -lgimp-2.0 -lgimpmath-2.0 -lgimpconfig-2.0 -lgimpcolor-2.0 -lgimpbase-2.0 -lgmodule-2.0 -lglib-2.0 -ljson-glib-1.0 -lbabl-0.1 -lgtk-x11-2.0 -lgdk-x11-2.0 -lpangocairo-1.0 -latk-1.0 -lcairo -lgdk_pixbuf-2.0 -lgio-2.0 -lpangoft2-1.0 -lpango-1.0 -lgobject-2.0 -lglib-2.0 -lharfbuzz -lfontconfig -lfreetype -pthread -lgexiv2

# Todo -> change it so it can be modified to install in any ones dirs
# Directory path variable
INSTALL_DIR = /home/sami/.config/GIMP/2.10/plug-ins

# Output binary variable
OUTPUT_BINARY = $(INSTALL_DIR)/contactsheet

# Check for the -w flag
ifdef WINDOWS
    CC = mingw32-gcc
    CFLAGS += -DWINDOWS
endif

all: contactsheet

contactsheet: contactsheet.c
	$(CC) $(CFLAGS) -o $(OUTPUT_BINARY) contactsheet.c $(LIBS)

clean:
	rm -f $(OUTPUT_BINARY)
