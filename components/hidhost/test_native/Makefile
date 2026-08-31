# The report descriptor parser, the gamepad decoder and the descriptors captured from real
# hardware live in the badgeteam/hid-host component, which the build pulls into
# managed_components. Point HID_HOST somewhere else to build against a checkout of it instead.
HID_HOST ?= ../../../managed_components/badgeteam__hid-host

CC = gcc
CFLAGS = -Wall -Wextra -Werror -g -I. -I../include -I$(HID_HOST)/include -I$(HID_HOST)/test_native
OBJS = test_hid_gamepad.o hid_gamepad.o hid_gamepad_decode.o hid_layout.o

test_hid_gamepad: $(OBJS)
	$(CC) -o $@ $(OBJS)

test_hid_gamepad.o: test_hid_gamepad.c $(HID_HOST)/test_native/test_descriptors.h \
                    $(HID_HOST)/test_native/test_reports.h ../include/hid_gamepad_c64.h esp_log.h

hid_gamepad.o: ../src/hid_gamepad.c ../include/hid_gamepad_c64.h $(HID_HOST)/include/hid_gamepad.h
	$(CC) $(CFLAGS) -c -o $@ $<

hid_gamepad_decode.o: $(HID_HOST)/src/hid_gamepad.c $(HID_HOST)/include/hid_gamepad.h
	$(CC) $(CFLAGS) -c -o $@ $<

hid_layout.o: $(HID_HOST)/src/hid_layout.c $(HID_HOST)/include/hid_layout.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(HID_HOST)/src/hid_layout.c:
	@echo "badgeteam/hid-host is not in $(HID_HOST)."
	@echo "Run a build first so the component manager fetches it, or set HID_HOST to a checkout of"
	@echo "https://github.com/badgeteam/esp32-component-hid-host"
	@false

.PHONY: test
test: test_hid_gamepad
	./test_hid_gamepad

.PHONY: clean
clean:
	rm -f *.o test_hid_gamepad
