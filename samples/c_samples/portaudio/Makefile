SRC_FILES = pdtest.c ~/addacs/pd-external/addacs_in.c ~/addacs/pd-external/addacs_out.c
LIBPD = ../../../libs/libpd.so
TARGET = pdtest

CFLAGS = -I../../../pure-data/src -I../../../libpd_wrapper -std=c99 -O3 -march=native -I ~/addacs
LDFLAGS = -lportaudio

.PHONY: clean clobber

$(TARGET): ${SRC_FILES:.c=.o} $(LIBPD)
	gcc -o $(TARGET) $^ $(LIBPD) $(LDFLAGS)

$(LIBPD):
	cd ../../.. && make

clean:
	rm -f *.o

clobber: clean
	rm -f $(TARGET)

