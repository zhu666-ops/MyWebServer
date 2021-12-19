CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: ../webserver/main.cpp  ../webserver/utiltimer/util_timer.cpp ../webserver/httpconnect/http_connect.cpp ../webserver/webserver.cpp ../webserver/conf/conf.cpp ../webserver/epoll/epoll.cpp ../webserver/threadpool/*.cpp ../webserver/lock/*.cpp
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread

clean:
	rm  -r server
