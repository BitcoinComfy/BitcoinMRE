#makefile - BitcoinMRE

CC=armcc
CXX=armcpp
CXXFLAGS=$(CFLAGS)
CFLAGS+= -W  -o0 -g -Ono-inline   -I .\include;.\include\service;.\include\component;.\ResID;.\src\app\widget;.\src\app\launcher;.\src\app\wallpaper;.\src\app\screen_lock;.\include\service;.\include\component;.\src\framework;.\src\framework\ui_core\base;.\src\framework\ui_core\mvc;.\src\framework\ui_core\pme;.\src\framework\mmi_core;.\src\ui_engine\vrt\interface;.\src\component;.\src\ui_engine\framework\xml;.\;.;.\src\tiny-AES-c;.\src\uBitcoin\src;.\src\uBitcoin\src\utility;.\src\uBitcoin\src\utility\trezor;.\src\reedsolomon-c;.\src\QRCode

LD=$(CXX) $(CXXFLAGS)

LDFLAGS=

LDFLAGS+= 

LIBS+=-lodbc32 -lodbccp32 -lmrewin32 -lmsimg32

TARGET=BitcoinMRE

.PHONY: all
all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $<

%.o: %.cxx
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $<

%.res: %.rc
	$(RC) $(CPPFLAGS) -o $@ -i $<

CHACHA20POLY1305= \

	./src/uBitcoin/src/utility/trezor/chacha20poly1305/chacha20poly1305.h \

	./src/uBitcoin/src/utility/trezor/chacha20poly1305/chacha_merged.c \

	./src/uBitcoin/src/utility/trezor/chacha20poly1305/ecrypt-config.h \

	./src/uBitcoin/src/utility/trezor/chacha20poly1305/ecrypt-machine.h \

	./src/uBitcoin/src/utility/trezor/chacha20poly1305/ecrypt-portable.h \

	./src/uBitcoin/src/utility/trezor/chacha20poly1305/ecrypt-sync.h \

	./src/uBitcoin/src/utility/trezor/chacha20poly1305/ecrypt-types.h \

	./src/uBitcoin/src/utility/trezor/chacha20poly1305/poly1305-donna.h

TINY-AES-C= \

	./src/tiny-AES-c/aes.c \

	./src/tiny-AES-c/aes.h \

	./src/tiny-AES-c/pkcs7_padding.c \

	./src/tiny-AES-c/pkcs7_padding.h

REEDSOLOMON-C= \

	./src/reedsolomon-c/rs.c \

	./src/reedsolomon-c/rs.h

QRCODE= \

	./src/QRCode/qrcode.c \

	./src/QRCode/qrcode.h

HEADER_FILES= \

	./bitcoin_mre.h \

	./BitcoinMRE.h \

	./mini_stdint.h \

	./mre_display.h \

	./mre_events.h \

	./mre_io.h \

	./mre_menu.h \

	./nuklear.h \

	./nuklear_mre.h \

	./share.h

RESOURCE_FILES= \

	./res/BitcoinMRE.res.xml

SRCS=$(CHACHA20POLY1305) $(TINY-AES-C) $(REEDSOLOMON-C) $(QRCODE) $(HEADER_FILES) $(RESOURCE_FILES) 

OBJS=$(patsubst %.cxx,%.o,$(patsubst %.cpp,%.o,$(patsubst %.cc,%.o,$(patsubst %.c,%.o,$(filter %.c %.cc %.cpp %.cxx ,$(SRCS))))))

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

.PHONY: clean
clean:
	-rm -f  $(OBJS) $(TARGET)

