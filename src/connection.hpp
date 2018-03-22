/*
 * Copyright 2017, alex at staticlibs.net
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* 
 * File:   connection.hpp
 * Author: alex
 *
 * Created on September 16, 2017, 8:14 PM
 */

#ifndef WILTON_USB_CONNECTION_HPP
#define WILTON_USB_CONNECTION_HPP

#include <string>

#include "staticlib/config.hpp"
#include "staticlib/io.hpp"
#include "staticlib/pimpl.hpp"

#include "usb_config.hpp"

namespace wilton {
namespace usb {

class connection : public sl::pimpl::object {
protected:
    /**
     * implementation class
     */
    class impl;

public:
    /**
     * PIMPL-specific constructor
     * 
     * @param pimpl impl object
     */
    PIMPL_CONSTRUCTOR(connection)

    connection(usb_config&& conf);

    std::string read(uint32_t length);

    uint32_t write(sl::io::span<const char> data);

    std::string control(const sl::json::value& control_options);

    static void initialize();
};

} // namespace
}

#endif /* WILTON_USB_CONNECTION_HPP */

