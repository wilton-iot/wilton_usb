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

#include "wilton/support/alloc_copy.hpp"
#include "wilton/support/buffer.hpp"

#include "connection.hpp"
#include "usb_config.hpp"

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
        auto usb = wilton::usb::connection(std::move(uconf));
        wilton_USB* usb_ptr = new wilton_USB(std::move(usb));
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
        std::string res = usb->impl().read(static_cast<uint32_t>(len));
        auto buf = wilton::support::make_string_buffer(res);
        *data_out = buf.value().data();
        *data_len_out = static_cast<int>(buf.value().size());
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
        uint32_t written = usb->impl().write({data, data_len});
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
        std::string res = usb->impl().control(copts);
        auto buf = wilton::support::make_string_buffer(res);
        *data_out = buf.value().data();
        *data_len_out = static_cast<int>(buf.value().size());
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}

char* wilton_USB_close(
        wilton_USB* usb) /* noexcept */ {
    if (nullptr == usb) return wilton::support::alloc_copy(TRACEMSG("Null 'usb' parameter specified"));
    delete usb;
    return nullptr;
}
