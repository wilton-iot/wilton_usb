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
};

} // namespace
}

#endif /* WILTON_USB_CONNECTION_HPP */

