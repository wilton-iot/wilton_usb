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
 * File:   wilton_usb.cpp
 * Author: alex
 *
 * Created on September 16, 2017, 8:13 PM
 */

#include "wilton/wilton_usb.h"

#include <memory>
#include <string>

#include "staticlib/config.hpp"

#include "wilton/support/alloc.hpp"
#include "wilton/support/buffer.hpp"
#include "wilton/support/handle_registry.hpp"
#include "wilton/support/logging.hpp"

#include "connection.hpp"
#include "usb_config.hpp"

namespace { // anonymous

const std::string logger = std::string("wilton.USB");

} // namespace

struct wilton_USB {
private:
    wilton::usb::connection usb;

public:
    wilton_USB(wilton::usb::connection&& usb) :
    usb(std::move(usb)) { }

    wilton::usb::connection& impl() {
        return usb;
    }
};

char* wilton_USB_open(
        wilton_USB** usb_out,
        const char* conf,
        int conf_len) /* noexcept */ {
    if (nullptr == usb_out) return wilton::support::alloc_copy(TRACEMSG("Null 'usb_out' parameter specified"));
    if (nullptr == conf) return wilton::support::alloc_copy(TRACEMSG("Null 'conf' parameter specified"));
    if (!sl::support::is_uint16_positive(conf_len)) return wilton::support::alloc_copy(TRACEMSG(
            "Invalid 'conf_len' parameter specified: [" + sl::support::to_string(conf_len) + "]"));
    try {
        auto conf_json = sl::json::load({conf, conf_len});
        auto uconf = wilton::usb::usb_config(conf_json);
        wilton::support::log_debug(logger, std::string("Opening USB connection,") +
                " VID: [" + sl::support::to_string(uconf.vendor_id) + "]," +
                " PID: [" + sl::support::to_string(uconf.product_id) + "]," +
                " timeout: [" + sl::support::to_string(uconf.timeout_millis) + "] ...");
        auto usb = wilton::usb::connection(std::move(uconf));
        wilton_USB* usb_ptr = new wilton_USB(std::move(usb));
        wilton::support::log_debug(logger, "Connection opened, handle: [" + wilton::support::strhandle(usb_ptr) + "]");
        *usb_out = usb_ptr;
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

char* wilton_USB_read(
        wilton_USB* usb,
        int len,
        char** data_out,
        int* data_len_out) /* noexcept */ {
    if (nullptr == usb) return wilton::support::alloc_copy(TRACEMSG("Null 'usb' parameter specified"));
    if (!sl::support::is_uint32_positive(len)) return wilton::support::alloc_copy(TRACEMSG(
            "Invalid 'len' parameter specified: [" + sl::support::to_string(len) + "]"));
    if (nullptr == data_out) return wilton::support::alloc_copy(TRACEMSG("Null 'data_out' parameter specified"));
    if (nullptr == data_len_out) return wilton::support::alloc_copy(TRACEMSG("Null 'data_len_out' parameter specified"));
    try {
        wilton::support::log_debug(logger, std::string("Reading from USB connection,") +
                " handle: [" + wilton::support::strhandle(usb) + "]," +
                " length: [" + sl::support::to_string(len) + "] ...");
        std::string res = usb->impl().read(static_cast<uint32_t>(len));
        wilton::support::log_debug(logger, std::string("Read operation complete,") +
                " bytes read: [" + sl::support::to_string(res.length()) + "]," +
                " data: [" + sl::io::format_plain_as_hex(res) + "]");
        auto buf = wilton::support::make_string_buffer(res);
        *data_out = buf.data();
        *data_len_out = buf.size_int();
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

char* wilton_USB_write(
        wilton_USB* usb,
        const char* data,
        int data_len,
        int* len_written_out) /* noexcept */ {
    if (nullptr == usb) return wilton::support::alloc_copy(TRACEMSG("Null 'usb' parameter specified"));
    if (nullptr == data) return wilton::support::alloc_copy(TRACEMSG("Null 'data' parameter specified"));
    if (!sl::support::is_uint32_positive(data_len)) return wilton::support::alloc_copy(TRACEMSG(
            "Invalid 'data_len' parameter specified: [" + sl::support::to_string(data_len) + "]"));
    try {
        auto data_src = sl::io::array_source(data, data_len);
        auto hex_sink = sl::io::string_sink();
        sl::io::copy_to_hex(data_src, hex_sink);
        wilton::support::log_debug(logger, std::string("Writing data to USB connection,") +
                " handle: [" + wilton::support::strhandle(usb) + "]," +
                " data: [" + sl::io::format_hex(hex_sink.get_string()) +  "],"
                " data_len: [" + sl::support::to_string(data_len) +  "] ...");
        uint32_t written = usb->impl().write({data, data_len});
        wilton::support::log_debug(logger, std::string("Write operation complete,") +
                " bytes written: [" + sl::support::to_string(written) + "]");
        *len_written_out = static_cast<int>(written);
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

char* wilton_USB_control(
        wilton_USB* usb,
        const char* options,
        int options_len,
        char** data_out,
        int* data_len_out) /* noexcept */ {
    if (nullptr == usb) return wilton::support::alloc_copy(TRACEMSG("Null 'usb' parameter specified"));
    if (nullptr == options) return wilton::support::alloc_copy(TRACEMSG("Null 'options' parameter specified"));
    if (!sl::support::is_uint16_positive(options_len)) return wilton::support::alloc_copy(TRACEMSG(
            "Invalid 'options_len' parameter specified: [" + sl::support::to_string(options_len) + "]"));
    if (nullptr == data_out) return wilton::support::alloc_copy(TRACEMSG("Null 'data_out' parameter specified"));
    if (nullptr == data_len_out) return wilton::support::alloc_copy(TRACEMSG("Null 'data_len_out' parameter specified"));
    try {
        auto copts = sl::json::load({options, options_len});
        wilton::support::log_debug(logger, std::string("Sending control command to USB connection,") +
                " handle: [" + wilton::support::strhandle(usb) + "]," +
                " options: [" + copts.dumps() +  "] ...");
        std::string res = usb->impl().control(copts);
        wilton::support::log_debug(logger, std::string("Control operation complete,") +
                " bytes read: [" + sl::support::to_string(res.length()) + "]," +
                " data: [" + sl::io::format_plain_as_hex(res) + "]");
        auto buf = wilton::support::make_string_buffer(res);
        *data_out = buf.data();
        *data_len_out = buf.size_int();
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

char* wilton_USB_close(
        wilton_USB* usb) /* noexcept */ {
    if (nullptr == usb) return wilton::support::alloc_copy(TRACEMSG("Null 'usb' parameter specified"));
    try {
        wilton::support::log_debug(logger, "Closing USB connection, handle: [" + wilton::support::strhandle(usb) + "] ...");
        delete usb;
        wilton::support::log_debug(logger, "Connection closed");
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}
