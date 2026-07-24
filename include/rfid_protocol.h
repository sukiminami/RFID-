#ifndef RFID_PROTOCOL_H
#define RFID_PROTOCOL_H

#include <stdint.h>
#include <Arduino.h>

#define RFID_FRAME_HEADER_1 0x52
#define RFID_FRAME_HEADER_2 0x46
#define RFID_MAX_FRAME_LEN 512
#define RFID_MAX_TAG_COUNT 20
#define RFID_MAX_TLV_COUNT 10

typedef enum {
    RFID_FRAME_TYPE_COMMAND = 0x00,
    RFID_FRAME_TYPE_RESPONSE = 0x01,
    RFID_FRAME_TYPE_NOTIFICATION = 0x02
} RfidFrameType;

typedef enum {
    RFID_CMD_REBOOT = 0x10,
    RFID_CMD_CONTINUOUS_INVENTORY = 0x21,
    RFID_CMD_SINGLE_INVENTORY = 0x22,
    RFID_CMD_STOP_INVENTORY = 0x23,
    RFID_CMD_WRITE_TAG = 0x30,
    RFID_CMD_READ_TAG = 0x31,
    RFID_CMD_QUERY_VERSION = 0x40,
    RFID_CMD_BATCH_SET_PARAMS = 0x41,
    RFID_CMD_SET_SINGLE_PARAM = 0x48,
    RFID_CMD_QUERY_SINGLE_PARAM = 0x49,
    RFID_CMD_RELAY_CONTROL = 0x4C,
    RFID_CMD_VOICE_PLAY = 0x4D,
    
    RFID_NOTIFY_TAG_REPORT = 0x80,
    RFID_NOTIFY_HISTORY_TAG = 0x81
} RfidFrameCode;

typedef enum {
    RFID_STATUS_SUCCESS = 0x00,
    RFID_STATUS_UNSUPPORTED_PARAM = 0x14,
    RFID_STATUS_PARAM_LEN_ERROR = 0x15,
    RFID_STATUS_PARAM_CONTENT_ERROR = 0x16,
    RFID_STATUS_UNSUPPORTED_CMD = 0x17,
    RFID_STATUS_ADDR_MISMATCH = 0x18,
    RFID_STATUS_CHECKSUM_ERROR = 0x20,
    RFID_STATUS_INVALID_TLV_TYPE = 0x21,
    RFID_STATUS_FLASH_WRITE_FAILED = 0x22,
    RFID_STATUS_INTERNAL_ERROR = 0xFF
} RfidStatus;

typedef enum {
    RFID_TLV_STATUS = 0x07,
    RFID_TLV_OPERATION = 0x08,
    RFID_TLV_WORKING_PARAMS = 0x23,
    RFID_TLV_SINGLE_PARAM = 0x26,
    RFID_TLV_TAG = 0x50,
    
    RFID_TLV_SUB_EPC = 0x01,
    RFID_TLV_SUB_RSSI = 0x05,
    RFID_TLV_SUB_TIMESTAMP = 0x06
} RfidTlvType;

typedef enum {
    RFID_MEM_BANK_RESERVED = 0x00,
    RFID_MEM_BANK_EPC = 0x01,
    RFID_MEM_BANK_TID = 0x02,
    RFID_MEM_BANK_USER = 0x03
} RfidMemoryBank;

typedef enum {
    RFID_OP_READ = 0x00,
    RFID_OP_WRITE = 0x01,
    RFID_OP_LOCK = 0x02,
    RFID_OP_KILL = 0x03
} RfidOperationType;

typedef enum {
    RFID_SINGLE_PARAM_POWER = 0x01,
    RFID_SINGLE_PARAM_BUZZER = 0x02,
    RFID_SINGLE_PARAM_FILTER_TIME = 0x03,
    RFID_SINGLE_PARAM_RF_GAIN = 0x04
} RfidSingleParamType;

typedef enum {
    RFID_WORK_MODE_PASSIVE = 0x00,
    RFID_WORK_MODE_ACTIVE = 0x01,
    RFID_WORK_MODE_TRIGGER = 0x02
} RfidWorkMode;

typedef struct {
    uint8_t type;
    uint8_t length;
    uint8_t value[256];
} RfidTlv;

typedef struct {
    uint8_t header[2];
    uint8_t frameType;
    uint16_t deviceAddr;
    uint8_t frameCode;
    uint16_t paramLen;
    RfidTlv tlvs[RFID_MAX_TLV_COUNT];
    uint8_t tlvCount;
    uint8_t checksum;
} RfidFrame;

typedef struct {
    uint8_t epcLen;
    uint8_t epc[256];
    int8_t rssi;
    uint8_t timestamp[6];
} RfidTagInfo;

typedef struct {
    uint8_t status;
    uint8_t tagCount;
    RfidTagInfo tags[RFID_MAX_TAG_COUNT];
    char version[64];
} RfidResponse;

uint8_t rfidCalcChecksum(const uint8_t *data, uint16_t len);
bool rfidVerifyChecksum(const uint8_t *data, uint16_t len);
uint16_t rfidBuildFrame(uint8_t *buffer, uint8_t frameType, uint16_t deviceAddr, uint8_t frameCode, const RfidTlv *tlvs, uint8_t tlvCount);
bool rfidParseFrame(const uint8_t *buffer, uint16_t len, RfidFrame *frame);
bool rfidExtractResponse(const RfidFrame *frame, RfidResponse *response);
bool rfidExtractNotification(const RfidFrame *frame, RfidResponse *response);

uint16_t rfidBuildSingleInventoryFrame(uint8_t *buffer, uint16_t deviceAddr);
uint16_t rfidBuildContinuousInventoryFrame(uint8_t *buffer, uint16_t deviceAddr);
uint16_t rfidBuildStopInventoryFrame(uint8_t *buffer, uint16_t deviceAddr);
uint16_t rfidBuildReadFrame(uint8_t *buffer, uint16_t deviceAddr, RfidMemoryBank bank, uint16_t startAddr, uint16_t wordCount, const uint8_t *accessPassword);
uint16_t rfidBuildWriteFrame(uint8_t *buffer, uint16_t deviceAddr, RfidMemoryBank bank, uint16_t startAddr, const uint8_t *data, uint16_t wordCount, const uint8_t *accessPassword);
uint16_t rfidBuildQueryVersionFrame(uint8_t *buffer, uint16_t deviceAddr);
uint16_t rfidBuildSetParamFrame(uint8_t *buffer, uint16_t deviceAddr, RfidSingleParamType paramType, const uint8_t *value, uint8_t valueLen);

void rfidPrintHex(const uint8_t *data, uint16_t len);
void rfidPrintTlv(const RfidTlv *tlv);
void rfidPrintFrame(const RfidFrame *frame);

#endif
