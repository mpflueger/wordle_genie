INCLUDE = 

all: wordle_genie

wordle_genie: wordle_genie.cc
	g++ -O3 --std=c++1z $(INCLUDE) wordle_genie.cc -o wordle_genie

clean:
	rm wordle_genie
