// main.cpp

#include <deque>
#include <string>
#include <iostream>

#include "window.h"

int main(int argc, const char* argv[])
{
    using namespace treenote_tui;
    
    // todo: parse command line args
    
    std::deque<std::string> args{ argv + 1 , argc + argv };

    int rv{ 0 };
    
    {
        window win{ window::create() };
        rv = win(args);
    }
    
    if (rv != 0)
    {
        using treenote::note;
        
        if (global_signal_status == SIGTERM)
            std::cout << strings::received("SIGTERM").str_view() << '\n';
        else if (global_signal_status == SIGHUP)
            std::cout << strings::received("SIGHUP").str_view() << '\n';
        else if (global_signal_status == SIGQUIT)
            std::cout << strings::received("SIGQUIT").str_view() << '\n';
        
        if (window::autosave_msg.has_value())
        {
            using file_msg = treenote::note::file_msg;
            
            std::cout << '\n';
            
            switch (*window::autosave_msg)
            {
                // TODO: replace "current_filename_.string()" with "current_filename" if/when P2845R0 is implemented
                case file_msg::none:
                case file_msg::does_not_exist:
                case file_msg::is_unreadable:
                    std::cout << strings::tree_autosave(window::autosave_path.string()).str_view() << '\n';
                    break;
                case file_msg::is_directory:
                    std::cout << strings::error_writing(window::autosave_path.string(), strings::is_directory.str_view()).str_view()
                              << '\n';
                    break;
                case file_msg::is_device_file:
                case file_msg::is_invalid_file:
                    std::cout << strings::error_writing(window::autosave_path.string(), strings::invalid_file.str_view()).str_view()
                              << '\n';
                    break;
                case file_msg::is_unwritable:
                    std::cout << strings::error_writing(window::autosave_path.string(), strings::permission_denied.str_view()).str_view()
                              << '\n';
                    break;
                case file_msg::unknown_error:
                    std::cout << strings::error_writing(window::autosave_path.string(), strings::unknown_error.str_view()).str_view()
                              << '\n';
                    break;
            }
        }
        
        return 1;
    }
    
    return 0;
}
