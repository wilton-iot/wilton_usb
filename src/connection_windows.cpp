/*
 * Copyright 2018, alex at staticlibs.net
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
 * File:   connection_windows.cpp
 * Author: alex
 *
 * Created on September 16, 2017, 8:15 PM
 */

#include "connection.hpp"

#include <functional>
#include <memory>
#include <sstream>
#include <tuple>
#include <vector>

#ifndef UNICODE
#define UNICODE
#endif // UNICODE
#ifndef _UNICODE
#define _UNICODE
#endif // _UNICODE
#ifndef NOMINMAX
#define NOMINMAX
#endif NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#include <windows.h>
extern "C" {
    #include <api/setupapi.h>
    #include <api/hidsdi.h>
}

#include "staticlib/json.hpp"
#include "staticlib/io.hpp"
#include "staticlib/ranges.hpp"
#include "staticlib/support.hpp"
#include "staticlib/pimpl/forward_macros.hpp"
#include "staticlib/utils.hpp"

#include "wilton/support/exception.hpp"
#include "wilton/support/misc.hpp"

namespace wilton {
namespace usb {

class connection::impl : public staticlib::pimpl::object::impl {
    usb_config conf;

    HANDLE handle = nullptr;
    HIDP_CAPS caps;
    

public:
    impl(usb_config&& conf) :
    conf(std::move(conf)) {
        this->handle = find_and_open_by_vid_pid(this->conf.vendor_id, this->conf.product_id);
        std::memset(std::addressof(this->caps), '\0', sizeof(this->caps));
        get_device_capabilities(this->handle, this->caps, this->conf.vendor_id, this->conf.product_id);
    }

    ~impl() STATICLIB_NOEXCEPT {
        if (nullptr != handle) {
            ::CloseHandle(handle);
        }
    }

    std::string read(connection&, uint32_t length_ret) {
        uint64_t start = sl::utils::current_time_millis_steady();
        uint64_t finish = start + conf.timeout_millis;
        uint64_t cur = start;
        std::string res;
        uint32_t length = length_ret + 1;
        for (;;) {
            // (err, bytes_read, flag)
            bool completion_called_flag = false;
            std::tuple<DWORD, DWORD, bool*> state = std::make_tuple(0, 0, std::addressof(completion_called_flag));
            OVERLAPPED overlapped;
            std::memset(std::addressof(overlapped), '\0', sizeof (overlapped)); 
            overlapped.hEvent = static_cast<void*>(std::addressof(state));

            // completion routine
            auto completion = static_cast<LPOVERLAPPED_COMPLETION_ROUTINE> ([](
                    DWORD err, DWORD bytes_read, LPOVERLAPPED overlapped_ptr) {
                auto state_ptr = static_cast<std::tuple<DWORD, DWORD, bool*>*>(overlapped_ptr->hEvent);
                std::get<0>(*state_ptr) = err;
                std::get<1>(*state_ptr) = bytes_read;
                *std::get<2>(*state_ptr) = true;
            });

            // prepare read
            uint32_t passed = static_cast<uint32_t> (cur - start);
            int rtm = static_cast<int> (conf.timeout_millis - passed);
            auto prev_len = res.length();
            res.resize(length);
            auto rlen = length - prev_len;

            // start read
            auto err_read = ::ReadFileEx(
                    this->handle,
                    static_cast<void*> (std::addressof(res.front()) + prev_len),
                    static_cast<DWORD> (rlen),
                    std::addressof(overlapped),
                    completion); 
            if (0 == err_read) throw support::exception(TRACEMSG(
                    "USB 'ReadFileEx' error, VID: [" + sl::support::to_string(this->conf.vendor_id) + "]," +
                    " PID: [" + sl::support::to_string(this->conf.product_id) + "]" +
                    " bytes to read: [" + sl::support::to_string(length) + "]" +
                    " bytes read: [" + sl::support::to_string(res.length()) + "]" +
                    " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));

            auto err_wait_read = ::SleepEx(rtm, TRUE);
            if (WAIT_IO_COMPLETION != err_wait_read || !completion_called_flag) {
                // cancel pending operation
                auto err_cancel = ::CancelIo(this->handle);
                if (0 == err_cancel) throw support::exception(TRACEMSG(
                        "USB 'CancelIo' error, VID: [" + sl::support::to_string(this->conf.vendor_id) + "]," +
                        " PID: [" + sl::support::to_string(this->conf.product_id) + "]" +
                        " bytes to read: [" + sl::support::to_string(length) + "]" +
                        " bytes read: [" + sl::support::to_string(res.length()) + "]" +
                        " completion called: [" + sl::support::to_string(completion_called_flag) + "]" +
                        " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));
                // wait for operation to be canceled
                auto err_wait_canceled = ::SleepEx(INFINITE, TRUE);
                if (WAIT_IO_COMPLETION != err_wait_canceled || !completion_called_flag) throw support::exception(TRACEMSG(
                        "USB 'SleepEx' error, VID: [" + sl::support::to_string(this->conf.vendor_id) + "]," +
                        " PID: [" + sl::support::to_string(this->conf.product_id) + "]" +
                        " bytes to read: [" + sl::support::to_string(length) + "]" +
                        " bytes read: [" + sl::support::to_string(res.length()) + "]" +
                        " completion called: [" + sl::support::to_string(completion_called_flag) + "]" +
                        " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));
            }

            // at this point completion routine must be called
            if (ERROR_SUCCESS == std::get<0>(state)) {
                // check for warnings
                DWORD read_checked = 0;
                overlapped.hEvent = 0;
                auto err_get = ::GetOverlappedResult(
                        this->handle,
                        std::addressof(overlapped),
                        std::addressof(read_checked),
                        TRUE);
                if (0 == err_get) throw support::exception(TRACEMSG(
                        "USB 'GetOverlappedResult' error, VID: [" + sl::support::to_string(this->conf.vendor_id) + "]," +
                        " PID: [" + sl::support::to_string(this->conf.product_id) + "]" +
                        " bytes to read: [" + sl::support::to_string(length) + "]" +
                        " bytes read: [" + sl::support::to_string(res.length()) + "]" +
                        " bytes completion: [" + sl::support::to_string(std::get<1>(state)) + "]" +
                        " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));

                auto read = static_cast<size_t>(read_checked > std::get<1>(state) ? read_checked : std::get<1>(state));
                res.resize(prev_len + read);
                if (res.length() >= length) {
                    break;
                }
            } else if (ERROR_OPERATION_ABORTED == std::get<0>(state)) {
                res.resize(prev_len);
            } else throw support::exception(TRACEMSG(
                    "USB 'FileIOCompletionRoutine' error, VID: [" + sl::support::to_string(this->conf.vendor_id) + "]," +
                    " PID: [" + sl::support::to_string(this->conf.product_id) + "]" +
                    " bytes to read: [" + sl::support::to_string(length) + "]" +
                    " bytes read: [" + sl::support::to_string(res.length()) + "]" +
                    " error: [" + sl::utils::errcode_to_string(std::get<0>(state)) + "]"));

            // check timeout
            cur = sl::utils::current_time_millis_steady();
            if (cur >= finish) {
                break;
            }
        }
        return res.length() > 0 ? res.substr(1) : std::string();
    }

    uint32_t write(connection&, sl::io::span<const char> data_req) {
        auto data_str = std::string();
        data_str.resize(data_req.size() + 1);
        std::memcpy(std::addressof(data_str.front()) + 1, data_req.data(), data_req.size());
        auto data = sl::io::make_span(std::addressof(data_str.front()), data_str.size());
        uint64_t start = sl::utils::current_time_millis_steady();
        uint64_t finish = start + conf.timeout_millis;
        uint64_t cur = start;
        size_t written = 0;
        for(;;) {
            // (err, bytes_written, flag)
            bool completion_called_flag = false;
            std::tuple<DWORD, DWORD, bool*> state = std::make_tuple(0, 0, std::addressof(completion_called_flag));
            OVERLAPPED overlapped;
            std::memset(std::addressof(overlapped), '\0', sizeof (overlapped)); 
            overlapped.hEvent = static_cast<void*>(std::addressof(state));

            // completion routine
            auto completion = static_cast<LPOVERLAPPED_COMPLETION_ROUTINE> ([](
                    DWORD err, DWORD bytes_written, LPOVERLAPPED overlapped_ptr) {
                auto state_ptr = static_cast<std::tuple<DWORD, DWORD, bool*>*>(overlapped_ptr->hEvent);
                std::get<0>(*state_ptr) = err;
                std::get<1>(*state_ptr) = bytes_written;
                *std::get<2>(*state_ptr) = true;
            });

            // prepare write
            uint32_t passed = static_cast<uint32_t> (cur - start);
            int wtm = static_cast<DWORD> (conf.timeout_millis - passed);
            auto msg = std::string(data.data() + written, data.size() - written);

            // start write
            auto err_write = ::WriteFileEx(
                    this->handle,
                    static_cast<void*> (std::addressof(msg.front())),
                    static_cast<DWORD> (msg.length()),
                    std::addressof(overlapped),
                    completion); 

            if (0 == err_write) throw support::exception(TRACEMSG(
                    "USB 'WriteFileEx' error, VID: [" + sl::support::to_string(this->conf.vendor_id) + "]," +
                    " PID: [" + sl::support::to_string(this->conf.product_id) + "]" +
                    " bytes left to write: [" + sl::support::to_string(msg.length()) + "]" +
                    " bytes written: [" + sl::support::to_string(written) + "]" +
                    " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));

            auto err_wait_written = ::SleepEx(wtm, TRUE);
            if (WAIT_IO_COMPLETION != err_wait_written || !completion_called_flag) {
                // cancel pending operation
                auto err_cancel = ::CancelIo(this->handle);
                if (0 == err_cancel) throw support::exception(TRACEMSG(
                        "USB 'CancelIo' error, VID: [" + sl::support::to_string(this->conf.vendor_id) + "]," +
                        " PID: [" + sl::support::to_string(this->conf.product_id) + "]" +
                        " bytes left to write: [" + sl::support::to_string(msg.length()) + "]" +
                        " bytes written: [" + sl::support::to_string(written) + "]" +
                        " completion called: [" + sl::support::to_string(completion_called_flag) + "]" +
                        " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));
                // wait for operation to be canceled
                auto err_wait_canceled = ::SleepEx(INFINITE, TRUE);
                if (WAIT_IO_COMPLETION != err_wait_canceled || !completion_called_flag) throw support::exception(TRACEMSG(
                        "USB 'SleepEx' error, VID: [" + sl::support::to_string(this->conf.vendor_id) + "]," +
                        " PID: [" + sl::support::to_string(this->conf.product_id) + "]" +
                        " bytes left to write: [" + sl::support::to_string(msg.length()) + "]" +
                        " bytes written: [" + sl::support::to_string(written) + "]" +
                        " completion called: [" + sl::support::to_string(completion_called_flag) + "]" +
                        " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));
            }

            // at this point completion routine must be called
            if (ERROR_SUCCESS == std::get<0>(state)) {
                // check for warnings
                DWORD written_checked = 0;
                overlapped.hEvent = 0;
                auto err_get = ::GetOverlappedResult(
                        this->handle,
                        std::addressof(overlapped),
                        std::addressof(written_checked),
                        TRUE);
                if (0 == err_get) throw support::exception(TRACEMSG(
                        "USB 'GetOverlappedResult' error, VID: [" + sl::support::to_string(this->conf.vendor_id) + "]," +
                        " PID: [" + sl::support::to_string(this->conf.product_id) + "]" +
                        " bytes left to write: [" + sl::support::to_string(msg.length()) + "]" +
                        " bytes written: [" + sl::support::to_string(written) + "]" +
                        " bytes completion: [" + sl::support::to_string(std::get<1>(state)) + "]" +
                        " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));

                written += static_cast<size_t>(written_checked > std::get<1>(state) ? written_checked : std::get<1>(state));
                // check everything written
                if (written >= data.size()) {
                    break;
                } 
            } else if (ERROR_OPERATION_ABORTED != std::get<0>(state)) throw support::exception(TRACEMSG(
                    "USB 'FileIOCompletionRoutine' error, VID: [" + sl::support::to_string(this->conf.vendor_id) + "]," +
                    " PID: [" + sl::support::to_string(this->conf.product_id) + "]" +
                    " bytes left to write: [" + sl::support::to_string(msg.length()) + "]" +
                    " bytes written: [" + sl::support::to_string(written) + "]" +
                    " error: [" + sl::utils::errcode_to_string(std::get<0>(state)) + "]"));

            // check timeout
            cur = sl::utils::current_time_millis_steady();
            if (cur >= finish) {
                break;
            }
        }
        return static_cast<uint32_t>(written);
        
    }

    std::string control(connection&, const sl::json::value& control_options) {
        // parse options
        auto rdata = std::ref(sl::utils::empty_string());
        auto rdatahex = std::ref(sl::utils::empty_string());
        for (const sl::json::field& fi : control_options.as_object()) {
            auto& name = fi.name();
            if ("data" == name) {
                rdata = fi.as_string_nonempty_or_throw(name);
            } else if ("dataHex" == name) {
                rdatahex = fi.as_string_nonempty_or_throw(name);
            }
        }
        if (rdata.get().length() > conf.buffer_size) throw support::exception(TRACEMSG(
                "Invalid parameter 'data', size: [" + sl::support::to_string(rdata.get().size()) + "]"));
        if (rdatahex.get().length() > conf.buffer_size) throw support::exception(TRACEMSG(
                "Invalid parameter 'dataHex', size: [" + sl::support::to_string(rdatahex.get().size()) + "]"));
        std::string data = !rdata.get().empty() ? rdata.get() : sl::io::string_from_hex(rdatahex.get());
        auto data_pass = std::string();
        data_pass.resize(data.length() + 1);
        data_pass[0] = '\0';
        std::memcpy(std::addressof(data_pass.front()) + 1, data.c_str(), data.length());
        auto err = ::HidD_SetFeature(
                this->handle,
                reinterpret_cast<void*>(std::addressof(data_pass.front())),
                this->caps.FeatureReportByteLength);
        if (0 == err) throw support::exception(TRACEMSG(
                "USB 'HidD_SetFeature' error, VID: [" + sl::support::to_string(this->conf.vendor_id) + "]," +
                " PID: [" + sl::support::to_string(this->conf.product_id) + "]" +
                " data: [" + sl::io::format_plain_as_hex(data) + "]" +
                " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));
        return data;
    }

    static void initialize() {
        // no-op
    }

private:
    static HANDLE find_and_open_by_vid_pid(uint16_t vid, uint16_t pid) {
        GUID hid_guid;
        std::memset(std::addressof(hid_guid), '\0', sizeof(hid_guid));
        ::HidD_GetHidGuid(std::addressof(hid_guid));

        HANDLE dev_info = ::SetupDiGetClassDevs(
                std::addressof(hid_guid),
                nullptr,
                nullptr,
                DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
        if (INVALID_HANDLE_VALUE == dev_info) throw support::exception(TRACEMSG(
                "USB 'SetupDiGetClassDevs' error, VID: [" + sl::support::to_string(vid) + "]," +
                " PID: [" + sl::support::to_string(pid) + "]" +
                " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));

        SP_DEVICE_INTERFACE_DATA dev_info_data;
        std::memset(std::addressof(dev_info_data), '\0', sizeof(dev_info_data));
        dev_info_data.cbSize = sizeof (dev_info_data);

        DWORD dev_idx = 0;

        auto vid_pid_list = std::vector<std::pair<uint16_t, uint16_t>>();
        for (;;) {
            auto err_enum = SetupDiEnumDeviceInterfaces(
                    dev_info,
                    nullptr,
                    std::addressof(hid_guid),
                    dev_idx,
                    std::addressof(dev_info_data));
            if (0 == err_enum) {
                auto errcode = ::GetLastError();
                if (ERROR_NO_MORE_ITEMS == errcode) {
                    break;
                }
                if (INVALID_HANDLE_VALUE == dev_info) throw support::exception(TRACEMSG(
                        "USB 'SetupDiEnumDeviceInterfaces' error, VID: [" + sl::support::to_string(vid) + "]," +
                        " PID: [" + sl::support::to_string(pid) + "]" +
                        " index: [" + sl::support::to_string(dev_idx) + "]" +
                        " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));
            }

            DWORD len = 0;
            auto err_detail_len = ::SetupDiGetDeviceInterfaceDetail(
                    dev_info,
                    std::addressof(dev_info_data),
                    nullptr,
                    0,
                    std::addressof(len),
                    nullptr);
            auto errcode_detail_len = ::GetLastError();
            if (!(0 == err_detail_len && ERROR_INSUFFICIENT_BUFFER == errcode_detail_len)) throw support::exception(TRACEMSG(
                    "USB 'SetupDiGetDeviceInterfaceDetail' length error, VID: [" + sl::support::to_string(vid) + "]," +
                    " PID: [" + sl::support::to_string(pid) + "]" +
                    " index: [" + sl::support::to_string(dev_idx) + "]" +
                    " error: [" + sl::utils::errcode_to_string(errcode_detail_len) + "]"));

            auto detail_data_mem = std::vector<char>();
            detail_data_mem.resize(static_cast<size_t>(len));
            auto detail_data = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(detail_data_mem.data());
            detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

            DWORD required = 0;
            auto err_detail = ::SetupDiGetDeviceInterfaceDetail(
                    dev_info,
                    std::addressof(dev_info_data),
                    detail_data,
                    len,
                    std::addressof(required),
                    nullptr);
            if (0 == err_detail) throw support::exception(TRACEMSG(
                    "USB 'SetupDiGetDeviceInterfaceDetail' error, VID: [" + sl::support::to_string(vid) + "]," +
                    " PID: [" + sl::support::to_string(pid) + "]" +
                    " index: [" + sl::support::to_string(dev_idx) + "]" +
                    " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));

            HANDLE handle = ::CreateFileW(
                    detail_data->DevicePath,
                    GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr,
                    OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED,
                    nullptr);
            if (INVALID_HANDLE_VALUE == handle) {
                auto errcode = ::GetLastError();
                if ((ERROR_ACCESS_DENIED == errcode) || (ERROR_SHARING_VIOLATION == errcode)) {
                    // skip the device
                    dev_idx += 1;
                    continue; 
                }
                throw support::exception(TRACEMSG(
                        "USB 'CreateFileW' error, VID: [" + sl::support::to_string(vid) + "]," +
                        " PID: [" + sl::support::to_string(pid) + "]" +
                        " index: [" + sl::support::to_string(dev_idx) + "]" +
                        " error: [" + sl::utils::errcode_to_string(errcode) + "]"));
            }

            HIDD_ATTRIBUTES attributes;
            std::memset(std::addressof(attributes), '\0', sizeof(attributes));
            attributes.Size = sizeof(attributes);

            auto err_attr = ::HidD_GetAttributes(
                    handle,
                    std::addressof(attributes));
            if (0 == err_attr) throw support::exception(TRACEMSG(
                    "USB 'HidD_GetAttributes' error, VID: [" + sl::support::to_string(vid) + "]," +
                    " PID: [" + sl::support::to_string(pid) + "]" +
                    " index: [" + sl::support::to_string(dev_idx) + "]" +
                    " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));

            if (attributes.VendorID == vid && attributes.ProductID == pid) {
                return handle;
            } else {
                vid_pid_list.emplace_back(attributes.VendorID, attributes.ProductID);
                ::CloseHandle(handle);
            }

            dev_idx += 1;
        }

        throw support::exception(TRACEMSG(
                "Cannot find USB device with VID: [" + tohex(vid) + "], PID: [" + tohex(pid) + "],"
                " found devices [" + print_vid_pid_list(vid_pid_list) + "]")); 
    }

    static void get_device_capabilities(HANDLE handle, HIDP_CAPS& caps, uint16_t vid, uint16_t pid) {
        PHIDP_PREPARSED_DATA ppd = nullptr;
        
        auto err_ppd = ::HidD_GetPreparsedData(
                handle,
                std::addressof(ppd));
        if (0 == err_ppd) throw support::exception(TRACEMSG(
                "USB 'HidD_GetPreparsedData' error, VID: [" + sl::support::to_string(vid) + "]," +
                " PID: [" + sl::support::to_string(pid) + "]" +
                " handle: [" + wilton::support::strhandle(handle) + "]" +
                " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));
        auto deferred = sl::support::defer([ppd] () STATICLIB_NOEXCEPT {
            ::HidD_FreePreparsedData(ppd);
        });

        auto err_caps = ::HidP_GetCaps(
                ppd,
                std::addressof(caps));
        if (0 == err_caps) throw support::exception(TRACEMSG(
                "USB 'HidP_GetCaps' error, VID: [" + sl::support::to_string(vid) + "]," +
                " PID: [" + sl::support::to_string(pid) + "]" +
                " handle: [" + wilton::support::strhandle(handle) + "]" +
                " error: [" + sl::utils::errcode_to_string(::GetLastError()) + "]"));
    }

    static std::string print_vid_pid_list(const std::vector<std::pair<uint16_t, uint16_t>>& list) {
        auto vec = sl::ranges::transform(list, [](const std::pair<uint16_t, uint16_t>& pa) -> sl::json::value {
            auto obj = std::vector<sl::json::field>();
            obj.emplace_back("vendorId", tohex(pa.first));
            obj.emplace_back("productId", tohex(pa.second));
            return sl::json::value(std::move(obj));
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
