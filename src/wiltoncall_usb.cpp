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
 * File:   wiltoncall_usb.cpp
 * Author: alex
 *
 * Created on September 16, 2017, 8:13 PM
 */
#include <memory>
#include <string>

#include "staticlib/config.hpp"
#include "staticlib/io.hpp"
#include "staticlib/json.hpp"
#include "staticlib/support.hpp"
#include "staticlib/utils.hpp"

#include "wilton/wilton.h"
#include "wilton/wiltoncall.h"
#include "wilton/wilton_usb.h"

#include "wilton/support/buffer.hpp"
#include "wilton/support/registrar.hpp"
#include "wilton/support/unique_handle_registry.hpp"

// for local statics init only
#include "connection.hpp"

namespace wilton {
namespace usb {

namespace { //anonymous

// initialized from wilton_module_init
std::shared_ptr<support::unique_handle_registry<wilton_USB>> usb_registry() {
    static auto registry = std::make_shared<support::unique_handle_registry<wilton_USB>>(
            [](wilton_USB* usb) STATICLIB_NOEXCEPT {
                wilton_USB_close(usb);
            });
    return registry;
}

} // namespace

support::buffer open(sl::io::span<const char> data) {
    wilton_USB* usb = nullptr;
    char* err = wilton_USB_open(std::addressof(usb), data.data(), static_cast<int>(data.size()));
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
    auto reg = usb_registry();
    int64_t handle = reg->put(usb);
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
    auto reg = usb_registry();
    wilton_USB* ser = reg->remove(handle);
    if (nullptr == ser) throw support::exception(TRACEMSG(
            "Invalid 'usbHandle' parameter specified"));
    // call wilton
    char* err = wilton_USB_close(ser);
    if (nullptr != err) {
        reg->put(ser);
        support::throw_wilton_error(err, TRACEMSG(err));
    }
    return support::make_null_buffer();
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
    auto reg = usb_registry();
    wilton_USB* ser = reg->remove(handle);
    if (nullptr == ser) throw support::exception(TRACEMSG(
            "Invalid 'usbHandle' parameter specified"));
    // call wilton
    char* out = nullptr;
    int out_len = 0;
    char* err = wilton_USB_read(ser, static_cast<int>(len),
            std::addressof(out), std::addressof(out_len));
    reg->put(ser);
    if (nullptr != err) {
        support::throw_wilton_error(err, TRACEMSG(err));
    }
    if (nullptr == out) {
        return support::make_null_buffer();
    }
    // return hex
    auto src = sl::io::array_source(out, out_len);
    return support::make_hex_buffer(src);
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
    std::string sdata = sl::io::string_from_hex(rdatahex.get());
    // get handle
    auto reg = usb_registry();
    wilton_USB* ser = reg->remove(handle);
    if (nullptr == ser) throw support::exception(TRACEMSG(
            "Invalid 'usbHandle' parameter specified"));
    // call wilton
    int written_out = 0;
    char* err = wilton_USB_write(ser, sdata.c_str(), 
            static_cast<int> (sdata.length()), std::addressof(written_out));
    reg->put(ser);
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
    auto reg = usb_registry();
    wilton_USB* usb = reg->remove(handle);
    if (nullptr == usb) throw support::exception(TRACEMSG(
            "Invalid 'usbHandle' parameter specified"));
    // call wilton
    char* out = nullptr;
    int out_len = 0;
    char* err = wilton_USB_control(usb, options.c_str(), static_cast<int> (options.length()),
            std::addressof(out), std::addressof(out_len));
    reg->put(usb);
    if (nullptr != err) {
        support::throw_wilton_error(err, TRACEMSG(err));
    }
    if (nullptr == out) {
        return support::make_null_buffer();
    }
    // return hex
    auto src = sl::io::array_source(out, out_len);
    return support::make_hex_buffer(src);
}

} // namespace
}

extern "C" char* wilton_module_init() {
    try {
        wilton::usb::usb_registry();
        wilton::usb::connection::initialize();
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
