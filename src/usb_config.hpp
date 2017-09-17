/* 
 * File:   usb_config.hpp
 * Author: alex
 *
 * Created on September 16, 2017, 8:14 PM
 */

#ifndef WILTON_USB_USB_CONFIG_HPP
#define WILTON_USB_USB_CONFIG_HPP

#include <cstdint>
#include <string>

#include "staticlib/config.hpp"
#include "staticlib/support.hpp"
#include "staticlib/json.hpp"

#include "wilton/support/exception.hpp"

namespace wilton {
namespace usb {

class usb_config {
public:
    uint16_t vendor_id = 0;
    uint16_t product_id = 0;
    uint32_t out_endpoint = 0;
    uint32_t in_endpoint = 0;
    uint32_t timeout_millis = 500;
    uint32_t buffer_size = 4096;

    usb_config(const usb_config&) = delete;

    usb_config& operator=(const usb_config&) = delete;

    usb_config(usb_config&& other) :
    vendor_id(other.vendor_id),
    product_id(other.product_id),
    out_endpoint(other.out_endpoint),
    in_endpoint(other.in_endpoint),
    timeout_millis(other.timeout_millis),
    buffer_size(other.buffer_size) { }

    usb_config& operator=(usb_config&& other) {
        vendor_id = other.vendor_id;
        product_id = other.product_id;
        out_endpoint = other.out_endpoint;
        in_endpoint = other.in_endpoint;
        timeout_millis = other.timeout_millis;
        buffer_size = other.buffer_size;
        return *this;
    }

    usb_config() { }
    
    usb_config(const sl::json::value& json) {
        for (const sl::json::field& fi : json.as_object()) {
            auto& name = fi.name();
            if ("vendorId" == name) {
                this->vendor_id = fi.as_uint16_positive_or_throw(name);
            } else if ("productId" == name) {
                this->product_id = fi.as_uint16_positive_or_throw(name);
            } else if ("outEndpoint" == name) {
                this->out_endpoint = fi.as_uint32_positive_or_throw(name);
            } else if ("inEndpoint" == name) {
                this->in_endpoint = fi.as_uint32_positive_or_throw(name);
            } else if ("timeoutMillis" == name) {
                this->timeout_millis = fi.as_uint32_positive_or_throw(name);
            } else {
                throw support::exception(TRACEMSG("Unknown 'usb_config' field: [" + name + "]"));
            }
        }
        if (0 == vendor_id) throw support::exception(TRACEMSG(
                "Invalid 'usb.vendorId' field: []"));
        if (0 == product_id) throw support::exception(TRACEMSG(
                "Invalid 'usb.roductId' field: []"));
        if (0 == out_endpoint) throw support::exception(TRACEMSG(
                "Invalid 'usb.outEndpoint' field: []"));
        if (0 == in_endpoint) throw support::exception(TRACEMSG(
                "Invalid 'usb.inEndpoint' field: []"));
    }

    sl::json::value to_json() const {
        return {
            { "vendorId", vendor_id },
            { "productId", product_id },
            { "outEndpoint", out_endpoint },
            { "inEndpoint", in_endpoint },
            { "timeoutMillis", timeout_millis }
        };
    }
};

} // namespace
}

#endif /* WILTON_USB_USB_CONFIG_HPP */

