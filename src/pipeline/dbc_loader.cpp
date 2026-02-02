#include "pipeline/dbc_loader.hpp"
#include "core/logger.hpp"
#include <cstring>

namespace wowee {
namespace pipeline {

DBCFile::DBCFile() = default;
DBCFile::~DBCFile() = default;

bool DBCFile::load(const std::vector<uint8_t>& dbcData) {
    if (dbcData.size() < sizeof(DBCHeader)) {
        LOG_ERROR("DBC data too small for header");
        return false;
    }

    // Read header
    const DBCHeader* header = reinterpret_cast<const DBCHeader*>(dbcData.data());

    // Verify magic
    if (std::memcmp(header->magic, "WDBC", 4) != 0) {
        LOG_ERROR("Invalid DBC magic: ", std::string(header->magic, 4));
        return false;
    }

    recordCount = header->recordCount;
    fieldCount = header->fieldCount;
    recordSize = header->recordSize;
    stringBlockSize = header->stringBlockSize;

    // Validate sizes
    uint32_t expectedSize = sizeof(DBCHeader) + (recordCount * recordSize) + stringBlockSize;
    if (dbcData.size() < expectedSize) {
        LOG_ERROR("DBC file truncated: expected ", expectedSize, " bytes, got ", dbcData.size());
        return false;
    }

    // Validate record size matches field count
    if (recordSize != fieldCount * 4) {
        LOG_WARNING("DBC record size mismatch: recordSize=", recordSize,
                    " but fieldCount*4=", fieldCount * 4);
    }

    LOG_DEBUG("Loading DBC: ", recordCount, " records, ",
              fieldCount, " fields, ", recordSize, " bytes/record, ",
              stringBlockSize, " string bytes");

    // Copy record data
    const uint8_t* recordStart = dbcData.data() + sizeof(DBCHeader);
    uint32_t totalRecordSize = recordCount * recordSize;
    recordData.resize(totalRecordSize);
    std::memcpy(recordData.data(), recordStart, totalRecordSize);

    // Copy string block
    const uint8_t* stringStart = recordStart + totalRecordSize;
    stringBlock.resize(stringBlockSize);
    if (stringBlockSize > 0) {
        std::memcpy(stringBlock.data(), stringStart, stringBlockSize);
    }

    loaded = true;
    idCacheBuilt = false;
    idToIndexCache.clear();

    return true;
}

const uint8_t* DBCFile::getRecord(uint32_t index) const {
    if (!loaded || index >= recordCount) {
        return nullptr;
    }

    return recordData.data() + (index * recordSize);
}

uint32_t DBCFile::getUInt32(uint32_t recordIndex, uint32_t fieldIndex) const {
    if (!loaded || recordIndex >= recordCount || fieldIndex >= fieldCount) {
        return 0;
    }

    const uint8_t* record = getRecord(recordIndex);
    if (!record) {
        return 0;
    }

    const uint32_t* field = reinterpret_cast<const uint32_t*>(record + (fieldIndex * 4));
    return *field;
}

int32_t DBCFile::getInt32(uint32_t recordIndex, uint32_t fieldIndex) const {
    return static_cast<int32_t>(getUInt32(recordIndex, fieldIndex));
}

float DBCFile::getFloat(uint32_t recordIndex, uint32_t fieldIndex) const {
    if (!loaded || recordIndex >= recordCount || fieldIndex >= fieldCount) {
        return 0.0f;
    }

    const uint8_t* record = getRecord(recordIndex);
    if (!record) {
        return 0.0f;
    }

    const float* field = reinterpret_cast<const float*>(record + (fieldIndex * 4));
    return *field;
}

std::string DBCFile::getString(uint32_t recordIndex, uint32_t fieldIndex) const {
    uint32_t offset = getUInt32(recordIndex, fieldIndex);
    return getStringByOffset(offset);
}

std::string DBCFile::getStringByOffset(uint32_t offset) const {
    if (!loaded || offset >= stringBlockSize) {
        return "";
    }

    // Find null terminator
    const char* str = reinterpret_cast<const char*>(stringBlock.data() + offset);
    const char* end = reinterpret_cast<const char*>(stringBlock.data() + stringBlockSize);

    // Find string length (up to null terminator or end of block)
    size_t length = 0;
    while (str + length < end && str[length] != '\0') {
        length++;
    }

    return std::string(str, length);
}

int32_t DBCFile::findRecordById(uint32_t id) const {
    if (!loaded) {
        return -1;
    }

    // Build ID cache if not already built
    if (!idCacheBuilt) {
        buildIdCache();
    }

    auto it = idToIndexCache.find(id);
    if (it != idToIndexCache.end()) {
        return static_cast<int32_t>(it->second);
    }

    return -1;
}

void DBCFile::buildIdCache() const {
    idToIndexCache.clear();

    for (uint32_t i = 0; i < recordCount; i++) {
        uint32_t id = getUInt32(i, 0);  // Assume first field is ID
        idToIndexCache[id] = i;
    }

    idCacheBuilt = true;
    LOG_DEBUG("Built DBC ID cache with ", idToIndexCache.size(), " entries");
}

} // namespace pipeline
} // namespace wowee
