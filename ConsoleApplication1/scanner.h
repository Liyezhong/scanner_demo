#pragma once

#ifndef __SCANNER_H_
#define __SCANNER_H_

#include <cstdint>
#include <queue>
#include <mutex>
#include <algorithm>
#include <Winsock.h>
#include <Windows.h>

#include <string>
#include "libusb.h"

#pragma pack(1)

#define VENDOR_ID      0x0483
#define PRODUCT_ID     0x5740
#define ENDPOINT_OUT   0x01
#define ENDPOINT_IN    0x81

struct scannermsg {
    uint16_t head; // pc: 0xAADD; device: 0xBBDD
#define QRCODE_DATA                      1
#define IMAGE_DATA                       2
#define SCAN_MODE                        3
#define PHOTO_MODE                       4
#define SLEEP                            5
#define WAKEUP                           6
#define LIGHTON                          7
#define LIGHTOFF                         8
#define ENTER                            9
#define AUTO_SCANNING_START           0x0a
#define AUTO_SCANNING_STOP            0x0b
#define ENABLE_SCANNER                0x0c
#define DISABLE_SCANNER               0x0d
#define TRIGGER_PHOTO                 0x0e
#define INQUIRE_STATUS                0x0f
    uint8_t type;
} ;


// for scan and photo
struct scannermsg_ext {
    struct scannermsg head;
    uint32_t len;
    uint16_t crc;
    uint8_t id;
    uint8_t buffer[];
};

struct scannermsg_inqure_status {
    struct scannermsg head;
    uint8_t errorno;
};

#pragma pack()


class ScannerImageQRcodeThread;



class Scanner
{
    friend class ScannerImageQRcodeThread;
public:
     Scanner();
    ~Scanner();

    static Scanner* GetInstance();

public:
    void PhotoMode();
    void BarcodeMode();
    void AutoScanningStart();
    void AutoScanningStop();
    void TriggerPhoto();
    void Sleep();
    void Wakeup();
    void LightOn();
    void LightOff();
    void EnableScanner();
    void DisableScanner();



private:
    void scanner_msg_packing(int msg_type);
    int send(uint8_t* msg, int len, int& act_len, int timeout);
    int receive(uint8_t* msg, int len, int& act_len, int timeout);

public:
    void attach();
    void detach();

public:

#define emit
typedef void (*_CmdResultTransfer)(int type);
typedef void (*_BarcodeTransfer)(int id, uint8_t* barcode, int len);
typedef void (*_ImageTransfer)(int id, uint8_t* image, int len);
#define E0        0
#define E1        1
#define E2        2
#define E3        3
typedef void (*_ErrorCodeTransfer)(int errorno);

    _CmdResultTransfer CmdResultTransfer;
    _BarcodeTransfer BarcodeTransfer;
    _ImageTransfer ImageTransfer;
    _ErrorCodeTransfer ErrorCodeTransfer;

private:
    libusb_context* ctx;
    struct libusb_device_handle* handle;
    ScannerImageQRcodeThread* imageQRcodeThread;


    std::mutex             scannermsgQueueLock;
    std::queue<scannermsg> scannermsgQueue;
};


class ScannerImageQRcodeThread
{
public:
    ScannerImageQRcodeThread(Scanner* _scanner)
        : scanner(_scanner), is_runnable(true)
    {}

public:
    void start();
    void close();
    bool isRunning();

public:
    Scanner* scanner;
    bool is_runnable;

private:
    void run();
private:
    std::thread thread;
};


#endif
