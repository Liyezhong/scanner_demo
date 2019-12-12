#include "pch.h"
#include <iostream>
#include "scanner.h"
#include <thread>

#pragma comment(lib,"Ws2_32.lib")

Scanner::Scanner() : handle(nullptr)
{
    int ret = libusb_init(nullptr);
    if (ret < 0)
        return;

    attach();
}


Scanner::~Scanner()
{
    libusb_detach_kernel_driver(handle, 0);

    int ret = libusb_set_configuration(handle, 0);
    if (ret < 0) {
        std::cout << "libusb_set_configuration error " << ret;
        return;
    }
    std::cout << "Successfully set usb configuration 1";
    ret = libusb_claim_interface(handle, 0);
    imageQRcodeThread->close();
    delete imageQRcodeThread;

    libusb_release_interface(handle, 0);
    libusb_close(handle);
    libusb_exit(nullptr);
}


Scanner* Scanner::GetInstance()
{
    static Scanner self;
    return &self;
}

void Scanner::PhotoMode()
{
    AutoScanningStop();
    scanner_msg_packing(PHOTO_MODE);
}

void Scanner::BarcodeMode()
{
    scanner_msg_packing(SCAN_MODE);
    AutoScanningStop();
}

void Scanner::AutoScanningStart()
{
    BarcodeMode();
    scanner_msg_packing(AUTO_SCANNING_START);
}

void Scanner::AutoScanningStop()
{
    scanner_msg_packing(AUTO_SCANNING_STOP);
}

void Scanner::TriggerPhoto()
{
    scanner_msg_packing(TRIGGER_PHOTO);
}

void Scanner::Sleep()
{
    scanner_msg_packing(SLEEP);
}

void Scanner::Wakeup()
{
    scanner_msg_packing(WAKEUP);
}

void Scanner::LightOn()
{
    scanner_msg_packing(LIGHTON);
}

void Scanner::LightOff()
{
    scanner_msg_packing(LIGHTOFF);
}

void Scanner::EnableScanner()
{
    scanner_msg_packing(ENABLE_SCANNER);
}

void Scanner::DisableScanner()
{
    scanner_msg_packing(DISABLE_SCANNER);
}

int Scanner::send(uint8_t* msg, int len, int& act_len, int timeout)
{
    if (handle == nullptr)
        return -1;
    return libusb_bulk_transfer(handle, ENDPOINT_OUT, msg, len, &act_len, timeout);
}

int Scanner::receive(uint8_t* msg, int len, int& act_len, int timeout)
{
    if (handle == nullptr)
        return -1;
    return libusb_bulk_transfer(handle, ENDPOINT_IN, msg, len, &act_len, timeout);
}

void Scanner::scanner_msg_packing(int msg_type)
{
    std::lock_guard<std::mutex> lock(scannermsgQueueLock);
    scannermsg msg = { htons(0xAADD), msg_type };
    scannermsgQueue.push(msg);
}

void Scanner::attach()
{
    if (handle) {
        libusb_close(handle);
        handle = nullptr;
    }
    handle = libusb_open_device_with_vid_pid(nullptr, VENDOR_ID, PRODUCT_ID);
    if (handle == nullptr) {
        std::cout << "Could not find/open Scanner HID device.";
        return;
    }

    std::cout << "Successfully find the Scanner HID device.";

    libusb_detach_kernel_driver(handle, 0);

    int ret = libusb_set_configuration(handle, 1);
    if (ret < 0) {
        std::cout << "libusb_set_configuration error " << ret;
        goto out;
    }
    std::cout << "Successfully set usb configuration 1";
    ret = libusb_claim_interface(handle, 1);
    if (ret < 0) {
        std::cout << "libusb_claim_interface error";
        goto out;
    }
    std::cout << "Successfully claimed interface";

    if (imageQRcodeThread == nullptr) {
        imageQRcodeThread = new ScannerImageQRcodeThread(this);
        imageQRcodeThread->start();
    }

    return;

out:
    libusb_close(handle);
    handle = nullptr;
    return;
}

void Scanner::detach()
{
    if (imageQRcodeThread->isRunning()) {
        imageQRcodeThread->close();
    }

    libusb_close(handle);
    handle = nullptr;

    delete imageQRcodeThread;
    imageQRcodeThread = nullptr;
}

void ScannerImageQRcodeThread::start()
{
    thread = std::thread(&ScannerImageQRcodeThread::run, this);
}

void ScannerImageQRcodeThread::close()
{
    is_runnable = false;
    thread.join();
}

bool ScannerImageQRcodeThread::isRunning()
{
    return ((thread.joinable() == true) && (thread.get_id() != std::this_thread::get_id()));
}

void ScannerImageQRcodeThread::run()
{
    uint8_t* buffer = new uint8_t[1024 * 1024 * 8];
    uint8_t* message = new uint8_t[512];
    int ret = 0;

    uint8_t ack1Cmd[3] = { 0xaa, 0xdd, 0x02 };
    uint8_t ack2Cmd[3] = { 0xaa, 0xdd, 0x01 };

    long len, count;
    int actualLength;
    int id_check = -1;
    int barcode_id_check = -1;

    struct scannermsg* msg = (struct scannermsg*)message;
    struct scannermsg_ext* msg_ext = (struct scannermsg_ext*)message;

    while (is_runnable == true) {
        do {
            {
                std::lock_guard<std::mutex> lock(scanner->scannermsgQueueLock);
                if (!scanner->scannermsgQueue.empty()) {
                    auto sendMsg = scanner->scannermsgQueue.front();
                    scanner->scannermsgQueue.pop();
                    ret = scanner->send((uint8_t*)&sendMsg, sizeof(sendMsg), actualLength, 0);
                    if (ret) {
                        printf("scanner send error happen!\n");
                        while (!scanner->scannermsgQueue.empty())
                            scanner->scannermsgQueue.pop();
                        goto error_retry;
                    }
                }
            }

            ret = scanner->receive(message, sizeof(scannermsg), actualLength, 500);
            if (ret != 0) {
                if (ret == LIBUSB_ERROR_TIMEOUT)
                    ret = 0;
                else
                    printf("scanner receive error happen!, ret: %d, %s\n", ret, libusb_error_name(ret));
                goto error_retry;
            }

            //msg->head = ntohs(msg->head);
            //if (actualLength != sizeof(struct scannermsg) || msg->head != 0xBBDD) {
            //    printf("unkonwn msg...\n");
            //    for (int i = 0; i < actualLength; i++) {
            //        printf("%02X ", message[i]);
            //    }
            //    printf("\n");
            //    continue;
            //}

            if (msg->type == INQUIRE_STATUS) {
                emit scanner->ErrorCodeTransfer(((struct scannermsg_inqure_status*)msg)->errorno);
            } else {
                emit scanner->CmdResultTransfer(msg->type);
            }
        } while ((msg->type != QRCODE_DATA) && (msg->type != IMAGE_DATA));

        msg_ext->len = ntohl(msg_ext->len);
        msg_ext->crc = ntohs(msg_ext->crc);
        if (msg->type == IMAGE_DATA) {
            len = msg_ext->len;

            count = 0;
            ret = scanner->send(ack1Cmd, sizeof(scannermsg), actualLength, 0);

            int retry = 0;

            do {
                ret = scanner->receive(buffer + count, ((len > 504) ? 504 : len), actualLength, 200);
                if (ret != 0) {
                    if (ret == LIBUSB_ERROR_TIMEOUT) {
                        if (retry++ == 10) {
                            ret = 0;
                            goto error_retry;
                        }
                    } else {
                        printf("scanner receive error happen! ret: %d, %s\n", ret, libusb_error_name(ret));
                        goto error_retry;
                    }
                }
                retry = 0;
                len -= actualLength;
                count += actualLength;
            } while (len > 0);
            if (ret == 0)
                scanner->send(ack2Cmd, sizeof(scannermsg), actualLength, 0);

            if (id_check != msg_ext->id) {
                printf("__ali: recv image: id: %d, sizeof(scannermsg): %d, count: %d", msg_ext->id, sizeof(scannermsg), count);
                emit scanner->ImageTransfer(msg_ext->id, buffer, count);
                id_check = msg_ext->id;
            }
            //    printf("image size: %uk, %u, timestamp: %ld ms \n", count /1024, count, (tv_end.tv_sec * 1000 + tv_end.tv_usec / 1000) - (tv_start.tv_sec * 1000 + tv_start.tv_usec / 1000));
        }

        if (msg->type == QRCODE_DATA) {
            len = msg_ext->len;
            count = 0;
            int retry = 0;
            do {
                ret = scanner->receive(buffer + count, (len > 504) ? 504 : len, actualLength, 200);
                if (ret != 0) {
                    if (retry++ == 3)
                        goto error_retry;
                }
                retry = 0;
                len -= actualLength;
                count += actualLength;
            } while (len > 0);
            if (ret == 0)
                scanner->send(ack2Cmd, sizeof(scannermsg), actualLength, 0);
            if (barcode_id_check != msg_ext->id) {
                emit scanner->BarcodeTransfer(msg_ext->id, buffer, count);
                barcode_id_check = msg_ext->id;
                //printf("__ali: recv barcode: id: %d\n", msg_ext->id);
            }
        }

    error_retry:
        if (ret == LIBUSB_ERROR_NO_DEVICE)
            break;
        if (ret) {
            // error happen!
            //::Sleep(100);
        }
    }

    delete buffer;
    delete message;
}
