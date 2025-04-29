# NOTE: This Makefile only build the WASM version designed for the web.

CXX = emcc
AR = emar
OUTPUT = imgui.js
IMGUI_DIR:=./src/app/3rdparty/imgui
3RD_PARTY_DIR:=./src/app/3rdparty
TENDRIL_DIR:=./src/tendril
APP_DIR:=./src/app

LIBS = -lGL
WEBGL_VER = -s USE_WEBGL2=1 -s USE_GLFW=3 -s FULL_ES3=1
USE_WASM = -s WASM=1

all: app
	
app: libtendril.a
	$(CXX)  ./src/app/build.cpp  libtendril.a  -std=c++11 -o $(OUTPUT) $(LIBS) $(WEBGL_VER) -O2 --preload-file resources@/ $(USE_WASM) -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -I$(3RD_PARTY_DIR) -I$(TENDRIL_DIR) -I$(APP_DIR)

libtendril.o: ./src/tendril/build.cpp
	$(CXX) -c -Wall -std=c++11 ./src/tendril/build.cpp -o libtendril.o

libtendril.a: libtendril.o
	$(AR) rcs libtendril.a libtendril.o

clean:
	rm -f $(OUTPUT)
	rm *.o
	rm *.a
