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
 * File:   wilton_usb.h
 * Author: alex
 *
 * Created on September 16, 2017, 8:06 PM
 */

#ifndef WILTON_USB_H
#define WILTON_USB_H

#include "wilton/wilton.h"

#ifdef __cplusplus
extern "C" {
#endif

struct wilton_USB;
typedef struct wilton_USB wilton_USB;

char* wilton_USB_open(
        wilton_USB** usb_out,
        const char* conf,
        int conf_len);

char* wilton_USB_read(
        wilton_USB* usb,
        int len,
        char** data_out,
        int* data_len_out);


char* wilton_USB_write(
        wilton_USB* usb,
        const char* data,
        int data_len,
        int* len_written_out);

char* wilton_USB_control(
        wilton_USB* usb,
        const char* data,
        int data_len,
        char** data_out,
        int* data_len_out);

char* wilton_USB_close(
        wilton_USB* usb);

#ifdef __cplusplus
}
#endif

#endif /* WILTON_USB_H */

