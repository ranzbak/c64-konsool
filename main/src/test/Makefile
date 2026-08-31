# Host side tests for the 6502 core. Two of them, and both have to pass:
#
#     make -C main/src/test run
#
# cpu_test runs Klaus Dormann's functional test suite, which checks what each
# instruction leaves behind and says nothing at all about how long it took.
# cycle_test checks the timing that suite is blind to, which is what fast
# loaders depend on. vic_test checks the pixels the VIC actually produces,
# which nothing else does: every other check on the VIC compares one build
# against another and can only say that something changed.
# Run just one with `make klaus`, `make cycles` or `make vic`.

CXX      ?= g++
CXXFLAGS ?= -std=gnu++11 -Wall -Wextra -O2 -g

KLAUS_SRCS  = cpu_test.cpp ../CPU6502.cpp
CYCLE_SRCS  = cycle_test.cpp ../CPU6502.cpp

# The VIC drags in the display and the SID, so it builds against the same shim
# the host build uses, and needs C++17 for it.
VIC_SRCS    = vic_test.cpp ../VIC.cpp ../sid/sid.cpp
VIC_FLAGS   = -std=gnu++17 -O2 -g -DHOST_BUILD -I.. -I../../../host/shim

.PHONY: run klaus cycles vic clean
run: klaus cycles vic

klaus: cpu_test
	./cpu_test 6502_functional_test.bin

cycles: cycle_test
	./cycle_test

cpu_test: $(KLAUS_SRCS) ../CPU6502.hpp
	$(CXX) $(CXXFLAGS) -I.. -o $@ $(KLAUS_SRCS)

cycle_test: $(CYCLE_SRCS) ../CPU6502.hpp
	$(CXX) $(CXXFLAGS) -I.. -o $@ $(CYCLE_SRCS)

vic: vic_test
	./vic_test

vic_test: $(VIC_SRCS) ../VIC.hpp
	$(CXX) $(VIC_FLAGS) -o $@ $(VIC_SRCS)

clean:
	rm -f cpu_test cycle_test vic_test
