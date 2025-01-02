pthash-example: pthash-example.cpp pthash-example.hpp
	$(CXX) -g -o$@ -Iinclude -Iexternal/essentials/include -I. -Iexternal/xxHash pthash-example.cpp 
