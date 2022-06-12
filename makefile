COMPILER = clang++

build:
	$(COMPILER) main.cpp -o fileSystem

check:
	$(COMPILER) -v

run:
	./fileSystem $(ARGS)

clean:
	rm fileSystem
