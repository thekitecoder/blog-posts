good: proceduresGood.cpp
	g++ $(CXXFLAGS) --std=c++20 -o ./build/good proceduresGood.cpp
	readlink -f ./build/good

good_run:
	./build/good

bad: proceduresBad.cpp
	g++ $(CXXFLAGS) --std=c++20 -o ./build/bad proceduresBad.cpp
	readlink -f ./build/bad

bad_run:
	./build/bad