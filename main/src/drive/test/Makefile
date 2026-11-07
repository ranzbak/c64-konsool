# Host side tests for the CBM DOS layer. No ESP-IDF needed:
#
#     make -C main/src/drive/test run

CXX      ?= g++
CXXFLAGS ?= -std=gnu++11 -Wall -Wextra -O1 -g -fsanitize=address,undefined

SRCS = dos_test.cpp ../CbmDos.cpp ../D64Disk.cpp

.PHONY: run clean gcr
run: dos_test gcr_test drive_test
	./dos_test
	./gcr_test
	./drive_test

gcr: gcr_test
	./gcr_test

drive: drive_test
	./drive_test

DRIVE_SRCS = drive_test.cpp ../Drive1541.cpp ../DiskController.cpp ../Gcr.cpp ../Via6522.cpp \
             ../D64Disk.cpp ../../CPU6502.cpp

drive_test: $(DRIVE_SRCS) ../Drive1541.hpp ../DiskController.hpp ../Via6522.hpp ../IecLines.hpp
	$(CXX) $(CXXFLAGS) -I../.. -o $@ $(DRIVE_SRCS)

gcr_test: gcr_test.cpp ../Gcr.cpp ../D64Disk.cpp ../Gcr.hpp ../DiskImage.hpp
	$(CXX) $(CXXFLAGS) -o $@ gcr_test.cpp ../Gcr.cpp ../D64Disk.cpp

dos_test: $(SRCS) ../CbmDos.hpp ../D64Disk.hpp ../DiskImage.hpp ../IecDevice.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

clean:
	rm -f dos_test gcr_test drive_test
