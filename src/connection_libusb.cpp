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
 * File:   connection_libusb.cpp
 * Author: alex
 *
 * Created on September 16, 2017, 8:14 PM
 */

#include "connection.hpp"

#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "libusb-1.0/libusb.h"

#include "staticlib/json.hpp"
#include "staticlib/io.hpp"
#include "staticlib/ranges.hpp"
#include "staticlib/support.hpp"
#include "staticlib/pimpl/forward_macros.hpp"
#include "staticlib/utils.hpp"

#include "wilton/support/exception.hpp"

namespace wilton {
namespace usb {

namespace { // anonymous

// initialized from wilton_module_init
std::shared_ptr<libusb_context> shared_context() {
    static std::shared_ptr<libusb_context> ctx = 
            []() -> std::unique_ptr<libusb_context, std::function<void(libusb_context*)>> {
                libusb_context* ctx = nullptr;
                auto err = libusb_init(std::addressof(ctx));
                if (LIBUSB_SUCCESS != err) {
                    throw support::exception(TRACEMSG(
                            "USB 'libusb_init' error, code: [" + sl::support::to_string(err) + "]"));
                }
                return std::unique_ptr<libusb_context, std::function<void(libusb_context*)>> (
                        ctx, [](libusb_context* ctx) {
                            libusb_exit(ctx);
                        });
            }();
    return ctx;
}

} // namespace

class connection::impl : public staticlib::pimpl::object::impl {
    usb_config conf;

    std::unique_ptr<libusb_device_handle, std::function<void(libusb_device_handle*)>> handle;
    
public:
    impl(usb_config&& conf) :
    conf(std::move(conf)),
    handle(find_and_open_by_vid_pid(this->conf.vendor_id, this->conf.product_id),
            [this](libusb_device_handle* ha) {
                libusb_release_interface(ha, 0);
                libusb_close(ha);
            }
    ) {
    }

    std::string read(connection&, uint32_t length) {
        uint64_t start = sl::utils::current_time_millis_steady();
        uint64_t finish = start + conf.timeout_millis;
        uint64_t cur = start;
        std::string res;
        for (;;) {
            auto prev_len = res.length();
            res.resize(length);
            uint32_t passed = static_cast<uint32_t> (cur - start);
            int read = -1;
            int err = libusb_bulk_transfer(
                    handle.get(),
                    conf.in_endpoint,
                    reinterpret_cast<unsigned char*>(std::addressof(res.front()) + prev_len),
                    static_cast<int>(length - prev_len),
                    std::addressof(read),
                    static_cast<unsigned int>(conf.timeout_millis - passed));
            if (LIBUSB_ERROR_TIMEOUT != err && (LIBUSB_SUCCESS != err || -1 == read)) {
                throw support::exception(TRACEMSG(
                        "USB 'libusb_bulk_transfer' error, code: [" + sl::support::to_string(err) + "]"));
            }
            if (LIBUSB_ERROR_TIMEOUT != err) {
                res.resize(prev_len + read);
                if (res.length() >= length) {
                    break;
                }
            } else { // read timeout
                res.resize(prev_len);
            }
            cur = sl::utils::current_time_millis_steady();
            if (cur >= finish) {
                break;
            }
        }
        return res;
    }

    uint32_t write(connection&, sl::io::span<const char> data) {
        uint64_t start = sl::utils::current_time_millis_steady();
        uint64_t finish = start + conf.timeout_millis;
        uint64_t cur = start;
        auto data_mut = std::string();
        data_mut.resize(data.size());
        std::memcpy(std::addressof(data_mut.front()), data.data(), data.size());
        size_t written = 0;
        for(;;) {
            uint32_t passed = static_cast<uint32_t> (cur - start);
            int wr = -1;
            auto wlen = data.size() - written;
            auto packet = reinterpret_cast<unsigned char*>(std::addressof(data_mut.front()) + written);
            int err = libusb_bulk_transfer(
                    handle.get(),
                    conf.out_endpoint,
                    packet,
                    static_cast<int>(wlen),
                    std::addressof(wr),
                    static_cast<unsigned int>(conf.timeout_millis - passed));
            if (0 != err || -1 == wr) {
                throw support::exception(TRACEMSG(
                        "USB 'libusb_bulk_transfer' error, code: [" + sl::support::to_string(err) + "]"));
            }
            written += static_cast<size_t>(wr);
            if (written >= data.size()) {
                break;
            }
            cur = sl::utils::current_time_millis_steady();
            if (cur >= finish) {
                break;
            }
        }
        return static_cast<uint32_t>(written);
    }

    // http://libusb.sourceforge.net/api-1.0/group__syncio.html#gadb11f7a761bd12fc77a07f4568d56f38
    std::string control(connection&, const sl::json::value& control_options) {
        // parse options
        uint8_t request_type = 0;
        uint8_t request = 0;
        uint16_t value = 0;
        uint16_t index = 0;
        auto rdata = std::ref(sl::utils::empty_string());
        auto rdatahex = std::ref(sl::utils::empty_string());
        for (const sl::json::field& fi : control_options.as_object()) {
            auto& name = fi.name();
            if ("requestType" == name) {
                request_type = fi.as_uint16_positive_or_throw(name);
            } else if ("request" == name) {
                request = fi.as_uint16_positive_or_throw(name);
            } else if ("value" == name) {
                value = fi.as_uint16_or_throw(name);
            } else if ("index" == name) {
                index = fi.as_uint16_or_throw(name);
            } else if ("data" == name) {
                rdata = fi.as_string_nonempty_or_throw(name);
            } else if ("dataHex" == name) {
                rdatahex = fi.as_string_nonempty_or_throw(name);
            } else {
                throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
            }
        }
        if (0 == request_type) throw support::exception(TRACEMSG(
                "Required parameter 'requestType' not specified"));
        if (0 == request) throw support::exception(TRACEMSG(
                "Required parameter 'request' not specified"));
        if (rdata.get().length() > conf.buffer_size) throw support::exception(TRACEMSG(
                "Invalid parameter 'data', size: [" + sl::support::to_string(rdata.get().size()) + "]"));
        if (rdatahex.get().length() > conf.buffer_size) throw support::exception(TRACEMSG(
                "Invalid parameter 'dataHex', size: [" + sl::support::to_string(rdatahex.get().size()) + "]"));
        std::string data = !rdata.get().empty() ? rdata.get() : sl::io::string_from_hex(rdatahex.get());

        // call device
        auto buf = std::string();
        buf.resize(conf.buffer_size);
        if (!data.empty()) {
            std::memcpy(std::addressof(buf.front()), data.data(), data.length());
        }
        auto transferred = libusb_control_transfer(
                handle.get(),
                static_cast<uint8_t> (request_type),
                static_cast<uint8_t> (request),
                value,
                index,
                reinterpret_cast<unsigned char*>(std::addressof(buf.front())),
                !data.empty() ? data.length() : buf.length(),
                conf.timeout_millis);
        if (transferred < 0) {
            throw support::exception(TRACEMSG(
                    "USB 'libusb_control_transfer' error, code: [" + sl::support::to_string(transferred) + "]"));
        }
        return data.substr(0, transferred);
    }
    
    static void initialize() {
        shared_context();
    }

private:
    static libusb_device_handle* find_and_open_by_vid_pid(uint16_t vid, uint16_t pid) {
        auto ctx = shared_context();
	struct libusb_device **devlist = nullptr;
        auto err_getlist = libusb_get_device_list(ctx.get(), std::addressof(devlist));
        if (err_getlist < 0) {
            throw support::exception(TRACEMSG(
                    "USB 'libusb_get_device_list' error, code: [" + sl::support::to_string(err_getlist) + "]"));
        }
        auto deferred = sl::support::defer([devlist] () STATICLIB_NOEXCEPT {
            libusb_free_device_list(devlist, 1);
        });
        size_t devlist_size = static_cast<size_t>(err_getlist);

        std::vector<std::pair<uint16_t, uint16_t>> vid_pid_list;
        for (size_t i = 0; i < devlist_size; i++) {
            struct libusb_device_descriptor desc;
            auto err_desc = libusb_get_device_descriptor(devlist[i], std::addressof(desc));
            if (LIBUSB_SUCCESS != err_desc) {
                throw support::exception(TRACEMSG(
                        "USB 'libusb_get_device_descriptor' error, code: [" + sl::support::to_string(err_desc) + "]"));
            }
            vid_pid_list.emplace_back(desc.idVendor, desc.idProduct);
            if (desc.idVendor == vid && desc.idProduct == pid) {
                return open_device(devlist[i]);
            }
        }
        throw support::exception(TRACEMSG(
                "Cannot find USB device with VID: [" + tohex(vid) + "], PID: [" + tohex(pid) + "],"
                " found devices [" + print_vid_pid_list(vid_pid_list) + "]"));
    }

    static libusb_device_handle* open_device(libusb_device* dev) {
        libusb_device_handle* ha = nullptr;
        // open device
        auto err_open = libusb_open(dev, std::addressof(ha));
        if (LIBUSB_SUCCESS != err_open) {
            throw support::exception(TRACEMSG(
                    "USB 'libusb_open' error, code: [" + sl::support::to_string(err_open) + "]"));
        }
        bool cancel_deferred = false;
        auto deferred = sl::support::defer([&cancel_deferred, ha] () STATICLIB_NOEXCEPT {
            if (!cancel_deferred) {
                libusb_close(ha);
            }
        });
        // detach kernel
        auto kd_active = libusb_kernel_driver_active(ha, 0);
        if (kd_active) {
            auto err = libusb_detach_kernel_driver(ha, 0);
            if (LIBUSB_SUCCESS != err) {
                throw support::exception(TRACEMSG(
                        "USB 'libusb_detach_kernel_driver' error, code: [" + sl::support::to_string(err) + "]"));
            }
        }
        // claim
        auto err = libusb_claim_interface(ha, 0); 
        if (LIBUSB_SUCCESS != err) {
            throw support::exception(TRACEMSG(
                    "USB 'libusb_claim_interface' error, code: [" + sl::support::to_string(err) + "]"));
        }
        cancel_deferred = true;
        return ha;
    }

    static std::string print_vid_pid_list(const std::vector<std::pair<uint16_t, uint16_t>>& list) {
        auto vec = sl::ranges::transform(list, [](const std::pair<uint16_t, uint16_t>& pa) {
            return sl::json::value({
                { "vendorId", tohex(pa.first) },
                { "productId", tohex(pa.second) }
            });
        }).to_vector();
        return sl::json::dumps(std::move(vec));
    }

    static std::string tohex(uint16_t num) {
        std::stringstream ss{};
        ss << "0x" << std::hex << num;
        return ss.str();
    }

};
PIMPL_FORWARD_CONSTRUCTOR(connection, (usb_config&&), (), support::exception)
PIMPL_FORWARD_METHOD(connection, std::string, read, (uint32_t), (), support::exception)
PIMPL_FORWARD_METHOD(connection, uint32_t, write, (sl::io::span<const char>), (), support::exception)
PIMPL_FORWARD_METHOD(connection, std::string, control, (const sl::json::value&), (), support::exception)
PIMPL_FORWARD_METHOD_STATIC(connection, void, initialize, (), (), support::exception)

} // namespace
}
