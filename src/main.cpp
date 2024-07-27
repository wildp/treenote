// main.cpp

#include <deque>
#include <string>

#include "window.h"

int main(int argc, const char* argv[])
{
    // todo: parse command line args
    
    std::deque<std::string> args{ argv + 1 , argc + argv };

    window win{ window::create() };
    
    return win(args);
}
