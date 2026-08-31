# Host side tests for the image readers. These need no ESP-IDF, just a C++
# compiler:
#
#     make -C main/src/images/test run

CXX      ?= g++
CXXFLAGS ?= -std=gnu++11 -Wall -Wextra -O1 -g -fsanitize=address,undefined

SRCS = image_test.cpp ../CbmImage.cpp ../T64Image.cpp ../D64Image.cpp ../../drive/D64Disk.cpp

.PHONY: run clean
run: image_test
	./image_test

image_test: $(SRCS) ../CbmImage.hpp ../T64Image.hpp ../D64Image.hpp
	$(CXX) $(CXXFLAGS) -I../.. -o $@ $(SRCS)

clean:
	rm -f image_test
