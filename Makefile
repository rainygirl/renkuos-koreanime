# Builds on RenkuOS itself. On 32-bit x86 use the primary compiler (gcc2, the
# plain `g++`): input_server, which loads the add-on, is a gcc2 program.
#
#   make            build the input method and koreanime-setup
#   make install    install them for this user and switch to Korean
#   make uninstall  remove them again
#   make test       run the composer's tests (any machine with a C++ compiler)

CXX ?= g++
BUILD := build
ADDON := $(BUILD)/KoreanIME
SETUP := $(BUILD)/koreanime-setup
INDICATOR := $(BUILD)/KoreanIndicator
INPUT_SERVER := /boot/system/servers/input_server
PRIVATE_HEADERS := /boot/system/develop/headers/private

METHODS_DIR := $(HOME)/config/non-packaged/add-ons/input_server/methods
INDICATOR_DIR := $(HOME)/config/non-packaged/add-ons/koreanime
BIN_DIR := $(HOME)/config/non-packaged/bin

.PHONY: all install uninstall test clean

all: $(ADDON) $(SETUP) $(INDICATOR)

# Linked against input_server, which provides BInputServerMethod.
$(ADDON): src/Hangul.cpp src/Hangul.h src/KoreanInputMethod.cpp src/Icons.h \
		src/Indicator.h
	mkdir -p $(BUILD)
	$(CXX) -O2 -Wall -Wno-multichar -shared -fPIC -Isrc -o $@ \
		src/Hangul.cpp src/KoreanInputMethod.cpp $(INPUT_SERVER) -lbe

# Loaded by Deskbar: shows the mode, scaled with the Deskbar.
$(INDICATOR): src/KoreanIndicator.cpp src/Indicator.h
	mkdir -p $(BUILD)
	$(CXX) -O2 -Wall -Wno-multichar -shared -fPIC -Isrc -o $@ $< -lbe

$(SETUP): src/KoreanSetup.cpp
	mkdir -p $(BUILD)
	$(CXX) -O2 -Wall -I$(PRIVATE_HEADERS)/locale -o $@ $< -lbe

install: all
	mkdir -p $(METHODS_DIR) $(BIN_DIR) $(INDICATOR_DIR)
	cp $(INDICATOR) $(INDICATOR_DIR)/KoreanIndicator
	cp $(ADDON) $(METHODS_DIR)/KoreanIME
	cp $(SETUP) $(BIN_DIR)/koreanime-setup
	$(BIN_DIR)/koreanime-setup

uninstall:
	rm -f $(METHODS_DIR)/KoreanIME $(BIN_DIR)/koreanime-setup \
		$(INDICATOR_DIR)/KoreanIndicator

test: $(BUILD)/hangul_test
	$(BUILD)/hangul_test

$(BUILD)/hangul_test: test/HangulTest.cpp src/Hangul.cpp src/Hangul.h
	mkdir -p $(BUILD)
	$(CXX) -O2 -Wall -Isrc -o $@ test/HangulTest.cpp src/Hangul.cpp

clean:
	rm -rf $(BUILD)
