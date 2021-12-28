CC:= g++

TARGET1:= server
TARGET2:= client

INCLUDE:= -I./ -I/opt/homebrew/Cellar/openssl@3/3.0.1/include
LIBS:= -lpthread -lstdc++ -L/usr/lib -L/opt/homebrew/Cellar/openssl@3/3.0.1/lib -lssl -lcrypto

CXXFLAGS:= -std=c++0x -g -Wall -D_REENTRANT

OBJECTS1 :=server.o Thread.o
OBJECTS2 :=client.o

$(TARGET1): $(OBJECTS1)
	$(CC) -o $(TARGET1) $(OBJECTS1) $(LIBS)

$(TARGET2): $(OBJECTS2)
	$(CC) -o $(TARGET2) $(OBJECTS2) $(LIBS)


%.o:%.cpp
	$(CC) -c $(CXXFLAGS) $(INCLUDE) $< -o $@

.PHONY : clean
clean:
	-rm -f $(OBJECTS1) $(TARGET1)
	-rm -f $(OBJECTS2) $(TARGET2)
