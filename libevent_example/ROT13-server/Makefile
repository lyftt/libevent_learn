TARGET=$(subst .cpp,,$(wildcard *.cpp))

all:$(subst .cpp,,$(wildcard *.cpp))

%:%.cpp
	g++ -o $@ $< -levent

.PHONY:clean

clean:
	rm $(TARGET)

