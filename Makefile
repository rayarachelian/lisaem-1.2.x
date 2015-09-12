build:   lisa/lisaem_wx.cpp
	./build.sh build 

clean:   lisa/lisaem_wx.cpp
	./build.sh clean

install: lisa/lisaem_wx.cpp
	./build.sh build  --install
