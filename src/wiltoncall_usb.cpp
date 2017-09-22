/* 
 * File:   wiltoncall_usb.cpp
 * Author: alex
 *
 * Created on September 16, 2017, 8:13 PM
 */
#include <string>

#include "staticlib/config.hpp"
#include "staticlib/crypto.hpp"
#include "staticlib/io.hpp"
#include "staticlib/json.hpp"
#include "staticlib/support.hpp"
#include "staticlib/utils.hpp"

#include "wilton/wilton.h"
#include "wilton/wiltoncall.h"
#include "wilton/wilton_usb.h"

#include "wilton/support/handle_registry.hpp"
#include "wilton/support/buffer.hpp"
#include "wilton/support/registrar.hpp"

namespace wilton {
namespace usb {

namespace { //anonymous

support::handle_registry<wilton_USB>& static_registry() {
    static support::handle_registry<wilton_USB> registry {
        [] (wilton_USB* conn) STATICLIB_NOEXCEPT {
            wilton_USB_close(conn);
        }};
    return registry;
}

} // namespace

support::buffer open(sl::io::span<const char> data) {
    wilton_USB* usb;
    char* err = wilton_USB_open(std::addressof(usb), data.data(), static_cast<int>(data.size()));
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    int64_t handle = static_registry().put(usb);
    return support::make_json_buffer({
        { "usbHandle", handle}
    });
}

support::buffer close(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("usbHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'usbHandle' not specified"));
    // get handle
    wilton_USB* ser = static_registry().remove(handle);
    if (nullptr == ser) throw support::exception(TRACEMSG(
            "Invalid 'usbHandle' parameter specified"));
    // call wilton
    char* err = wilton_USB_close(ser);
    if (nullptr != err) {
        static_registry().put(ser);
        support::throw_wilton_error(err, TRACEMSG(err));
    }
    return support::make_empty_buffer();
}

support::buffer read(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    int64_t len = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("usbHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else if ("length" == name) {
            len = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'usbHandle' not specified"));
    if (-1 == len) throw support::exception(TRACEMSG(
            "Required parameter 'length' not specified"));
    // get handle
    wilton_USB* ser = static_registry().remove(handle);
    if (nullptr == ser) throw support::exception(TRACEMSG(
            "Invalid 'usbHandle' parameter specified"));
    // call wilton
    char* out = nullptr;
    int out_len = 0;
    char* err = wilton_USB_read(ser, static_cast<int>(len),
            std::addressof(out), std::addressof(out_len));
    static_registry().put(ser);
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::wrap_wilton_buffer(out, out_len);
}


support::buffer write(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    auto rdatahex = std::ref(sl::utils::empty_string());
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("usbHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else if ("dataHex" == name) {
            rdatahex = fi.as_string_nonempty_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'usbHandle' not specified"));
    if (rdatahex.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'dataHex' not specified"));
    std::string sdata = sl::crypto::from_hex(rdatahex.get());
    // get handle
    wilton_USB* ser = static_registry().remove(handle);
    if (nullptr == ser) throw support::exception(TRACEMSG(
            "Invalid 'usbHandle' parameter specified"));
    // call wilton
    int written_out = 0;
    char* err = wilton_USB_write(ser, sdata.c_str(), 
            static_cast<int> (sdata.length()), std::addressof(written_out));
    static_registry().put(ser);
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::make_json_buffer({
        { "bytesWritten", written_out }
    });
}

support::buffer control(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    auto options = std::string();
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("usbHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else if ("options" == name && sl::json::type::object == fi.json_type()) {
            options = fi.val().dumps();
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'usbHandle' not specified"));
    if (options.empty()) throw support::exception(TRACEMSG(
            "Required parameter 'options' not specified"));
    // get handle
    wilton_USB* usb = static_registry().remove(handle);
    if (nullptr == usb) throw support::exception(TRACEMSG(
            "Invalid 'usbHandle' parameter specified"));
    // call wilton
    char* out = nullptr;
    int out_len = 0;
    char* err = wilton_USB_control(usb, options.c_str(), static_cast<int> (options.length()),
            std::addressof(out), std::addressof(out_len));
    static_registry().put(usb);
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    return support::wrap_wilton_buffer(out, out_len);
}

} // namespace
}

extern "C" char* wilton_module_init() {
    try {
        wilton::support::register_wiltoncall("usb_open", wilton::usb::open);
        wilton::support::register_wiltoncall("usb_close", wilton::usb::close);
        wilton::support::register_wiltoncall("usb_read", wilton::usb::read);
        wilton::support::register_wiltoncall("usb_write", wilton::usb::write);
        wilton::support::register_wiltoncall("usb_control", wilton::usb::control);
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}
