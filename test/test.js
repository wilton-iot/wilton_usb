
// valgrind --track-fds=yes --leak-check=yes --show-reachable=yes --track-origins=yes --suppressions=../deps/cmake/resources/valgrind/popt_poptGetNextOpt.supp --suppressions=../deps/cmake/resources/valgrind/wilton_dyload.supp --suppressions=../deps/cmake/resources/valgrind/openssl_malloc.supp ./bin/wilton_cli ../modules/wilton_usb/test/test.js -m ../js

define([
    "wilton/hex",
    "wilton/USB"
], function(hex, USB) {

    function readAll(usb) {
        var resp = "";
        do {
            resp = usb.read(8);
            if (resp.length > 0) {
                print(hex.prettify(resp) + " [" + hex.decodeBytes(resp) + "]");
            }
        } while (resp.length > 0);
    }

    return {
        main: function() {
            // https://github.com/openyou/libomron/blob/master/examples/omron_720IT_test/omron_720IT_test.c

            var usb = new USB({
                vendorId: 0x0590, 
                productId: 0x0028,
                outEndpoint: 0x02,
                inEndpoint: 0x81,
                timeoutMillis: 1000 
            });

            var data = "";
            data += String.fromCharCode((0x0102 & 0xff00) >> 8);
            data += String.fromCharCode((0x0102 & 0x00ff));

            var cresp = usb.control({
                requestType: (0x01 << 5) | 0x01,
                request: 0x09,
                value: 3 << 8 | 0,
                index: 0,
                data: data
            });

            print("[" + cresp.length + "]");

            var INIT_HEAD = "07 00 00 00 00 00 00 00";
            var INIT_TAIL = "05 00 00 00 00 00 00 00";

            // init
            usb.writeHex(INIT_HEAD);
            usb.writeHex(INIT_TAIL);
            readAll(usb);
            usb.writeHex(INIT_HEAD);
            usb.writeHex(INIT_TAIL);
            readAll(usb);
            usb.writeHex(INIT_HEAD);
            usb.writeHex(INIT_TAIL);
            readAll(usb);
            
            // VER00
            usb.writeHex("05 56 45 52 30 30 00 00");
            readAll(usb);

            usb.close();
        }
    };
});
