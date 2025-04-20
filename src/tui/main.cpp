// tui/main.cpp
//
// Copyright (C) 2025 Peter Wild
//
// This file is part of tred.
//
// tred is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// tred is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with tred.  If not, see <https://www.gnu.org/licenses/>.


#include <deque>
#include <iostream>
#include <string>

#include "window.hpp"

int main(const int argc, const char* argv[])
{
    using namespace tred::tui;
    
    std::deque<std::string> args{ argv + 1 , argc + argv };

    int rv{ 0 };
    
    {
        window win{ window::create() };
        rv = win(args);
    }
    
    if (rv != 0)
    {
        using tred::core::editor;
        
        if (global_signal_status == SIGTERM)
            std::cout << strings::received("SIGTERM").str_view() << '\n';
        else if (global_signal_status == SIGHUP)
            std::cout << strings::received("SIGHUP").str_view() << '\n';
        else if (global_signal_status == SIGQUIT)
            std::cout << strings::received("SIGQUIT").str_view() << '\n';
        
        if (window::autosave_msg.has_value())
        {
            using file_msg = editor::file_msg;
            
            std::cout << '\n';
            
            switch (*window::autosave_msg)
            {
                case file_msg::none:
                case file_msg::does_not_exist:
                case file_msg::is_unreadable:
                    std::cout << strings::tree_autosave(window::autosave_path).str_view() << '\n';
                    break;
                case file_msg::is_directory:
                    std::cout << strings::error_writing(window::autosave_path, strings::is_directory.str_view()).str_view()
                              << '\n';
                    break;
                case file_msg::is_device_file:
                case file_msg::is_invalid_file:
                    std::cout << strings::error_writing(window::autosave_path, strings::invalid_file.str_view()).str_view()
                              << '\n';
                    break;
                case file_msg::is_unwritable:
                    std::cout << strings::error_writing(window::autosave_path, strings::permission_denied.str_view()).str_view()
                              << '\n';
                    break;
                case file_msg::unknown_error:
                    std::cout << strings::error_writing(window::autosave_path, strings::unknown_error.str_view()).str_view()
                              << '\n';
                    break;
            }
        }
        
        return 1;
    }
    
    return 0;
}
