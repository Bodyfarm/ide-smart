INSTALL_PREFIX=/usr/local

#CC      = gcc
PROF    = -g0 -O2
CFLAGS  = $(PROF) -Wall
LDFLAGS = $(PROF)
TARGET  = ide-smart
OBJECTS = ide-smart.o
		
ide-smart: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJECTS)

all: ide-smart

clean:
		rm -f $(OBJECTS) $(TARGET)
install:
		install -d $(INSTALL_PREFIX)/sbin
		install -d $(INSTALL_PREFIX)/man/man8
		install -s $(TARGET) $(INSTALL_PREFIX)/sbin
		install ide-smart.8 $(INSTALL_PREFIX)/man/man8
