# Builds the emulator for a normal machine. Headless: it writes the screen to a
# .ppm rather than driving a panel, so a demo can be run unattended.
#
#     make -C host
#     ./host/c64host --frames 400 --screenshot /tmp/ready.ppm

SRC  := ../main/src
CXX  ?= g++
CXXFLAGS ?= -std=gnu++17 -O2 -g -MMD -MP -DHOST_BUILD -I shim -I $(SRC) -I $(SRC)/drive
LDFLAGS  ?= -pthread

OBJS := c64host.o CPUC64.o VIC.o CIA.o CPU6502.o Joystick.o \
        Drive1541.o DiskController.o Gcr.o Via6522.o D64Disk.o CbmDos.o IecBus.o \
        MenuDataStore.o sid.o

vpath %.cpp $(SRC) $(SRC)/drive $(SRC)/menuoverlay $(SRC)/sid

all: c64host

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

c64host: $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

-include $(OBJS:.o=.d)

clean:
	rm -f *.o *.d c64host
