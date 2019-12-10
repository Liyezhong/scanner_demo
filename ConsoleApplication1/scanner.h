#pragma once

#include <cstdint>

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

#pragma pack()

#define SCANNER_MSG_PACKING(msg_type)                         \
        ({                                                    \
            QMutexLocker locker(&scannermsgQueueLock);        \
            scannermsg msg = {                                \
                .head = htons(0xAADD),                        \
                .type = (msg_type),                           \
            };                                                \
            scannermsgQueue.enqueue(msg);                     \
         })


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




class ScannerMonitorThread : public QThread
{
    Q_OBJECT
public:
    ScannerMonitorThread()
    {}
    void run() override;
};

class ScannerBase : public QObject
{
    Q_OBJECT
        friend class ScannerImageQRcodeThread;
public:

public:
    virtual void PhotoMode() = 0;
    virtual void BarcodeMode() = 0;
    virtual void AutoScanningStart() = 0;
    virtual void AutoScanningStop() = 0;
    virtual void TriggerPhoto() = 0;
    virtual void Sleep() = 0;
    virtual void Wakeup() = 0;
    virtual void LightOn() = 0;
    virtual void LightOff() = 0;
    virtual void EnableScanner() = 0;
    virtual void DisableScanner() = 0;

private:
    virtual int send(uint8_t* msg, int len, int& act_len, int timeout) = 0;
    virtual int receive(uint8_t* msg, int len, int& act_len, int timeout) = 0;

signals:
    void CmdResultTransfer(int type);
    void BarcodeTransfer(int id, uint8_t* barcode, int len);
    void ImageTransfer(int id, uint8_t* image, int len);
    /**
        E0 Device is good
        E1 Camera module is not work
        E2 Scanner module is not work
        E3 Others (IBD could define detail)
    */
#define E0        0
#define E1        1
#define E2        2
#define E3        3
    void ErrorCodeTransfer(int errorno);
};

class Scanner : public ScannerBase
{
    friend class ScannerMonitorThread;
public:
    explicit Scanner();
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
    int send(uint8_t* msg, int len, int& act_len, int timeout);
    int receive(uint8_t* msg, int len, int& act_len, int timeout);

    void attach();
    void detach();

public:
    static int LIBUSB_CALL usb_event_callback(libusb_context* ctx, libusb_device* dev, libusb_hotplug_event event, void* user_data);

signals:
    void CmdResultTransfer(int type);
    void BarcodeTransfer(int id, uint8_t* barcode, int len);
    void ImageTransfer(int id, uint8_t* image, int len);

    void ErrorCodeTransfer(int errorno);

private:
    libusb_context* ctx;
    struct libusb_device_handle* handle;
    ScannerImageQRcodeThread* imageQRcodeThread;
    ScannerMonitorThread* monitorThread;


    QMutex             scannermsgQueueLock;
    QQueue<scannermsg> scannermsgQueue;
};











class Scanner
{
};

