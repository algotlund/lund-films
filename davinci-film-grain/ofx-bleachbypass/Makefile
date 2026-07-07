CXX = clang++
SDK_ROOT = ../ofx-sdk-src
BUNDLE_NAME = BleachBypass.ofx.bundle
BINARY_NAME = BleachBypass.ofx

CXXFLAGS = -std=c++14 -O2 -fvisibility=hidden \
  -I$(SDK_ROOT)/include \
  -I$(SDK_ROOT)/Support/include \
  -I$(SDK_ROOT)/Support/Plugins/include \
  -arch arm64

LINKFLAGS = -bundle -fvisibility=hidden -framework OpenGL -arch arm64

SUPPORT_SRCS = \
  $(SDK_ROOT)/Support/Library/ofxsCore.cpp \
  $(SDK_ROOT)/Support/Library/ofxsImageEffect.cpp \
  $(SDK_ROOT)/Support/Library/ofxsInteract.cpp \
  $(SDK_ROOT)/Support/Library/ofxsLog.cpp \
  $(SDK_ROOT)/Support/Library/ofxsMultiThread.cpp \
  $(SDK_ROOT)/Support/Library/ofxsParams.cpp \
  $(SDK_ROOT)/Support/Library/ofxsProperty.cpp \
  $(SDK_ROOT)/Support/Library/ofxsPropertyValidation.cpp

SUPPORT_OBJS = $(patsubst %.cpp,build/%.o,$(notdir $(SUPPORT_SRCS)))
PLUGIN_OBJ = build/bleachbypass.o

all: $(BUNDLE_NAME)

build:
	mkdir -p build

build/%.o: $(SDK_ROOT)/Support/Library/%.cpp | build
	$(CXX) -c $(CXXFLAGS) $< -o $@

build/bleachbypass.o: bleachbypass.cpp | build
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BINARY_NAME): $(PLUGIN_OBJ) $(SUPPORT_OBJS)
	$(CXX) $^ $(LINKFLAGS) -o $@

$(BUNDLE_NAME): $(BINARY_NAME) Info.plist
	mkdir -p $(BUNDLE_NAME)/Contents/MacOS
	cp $(BINARY_NAME) $(BUNDLE_NAME)/Contents/MacOS/
	cp Info.plist $(BUNDLE_NAME)/Contents/

clean:
	rm -rf build $(BINARY_NAME) $(BUNDLE_NAME)

.PHONY: all clean
