// FileManager
// Rob Dobson 2018

#include "FileManager.h"

#include <sys/stat.h>

#include "ConfigPinMap.h"
#include "RdJson.h"
#include "Utils.h"
#include "driver/sdmmc_defs.h"
#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "vfs_api.h"

using namespace fs;

static const char* TAG = "FileManager";

void FileManager::setup(ConfigBase& config, const char* pConfigPath) {
    // Init
    _spiffsIsOk = false;
    _sdIsOk = false;
    _cachedFileListValid = false;

    // Get config
    String pathStr = "fileManager";
    if (pConfigPath) pathStr = pConfigPath;
    ConfigBase fsConfig(config.getString(pathStr.c_str(), "").c_str());
    ESP_LOGD(TAG, "setup with config: %s", fsConfig.getConfigCStrPtr());

    // See if SPIFFS enabled
    _enableSPIFFS = fsConfig.getLong("spiffsEnabled", 0) != 0;

    // Init SPIFFS if required
    if (_enableSPIFFS) {
        // Begin SPIFFS using arduino functions as web server relies on that file system
        bool spiffsFormatIfCorrupt = fsConfig.getLong("spiffsFormatIfCorrupt", 0) != 0;

        // Using ESP32 native SPIFFS support rather than arduino as potential bugs encountered in some
        // arduino functions
        esp_vfs_spiffs_conf_t conf = {
            .base_path = "/spiffs", .partition_label = NULL, .max_files = 5, .format_if_mount_failed = spiffsFormatIfCorrupt};
        // Use settings defined above to initialize and mount SPIFFS filesystem.
        // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
        esp_err_t ret = esp_vfs_spiffs_register(&conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "setup failed to register SPIFFS!");
            esp_restart();
        } else {
            // Get SPIFFS info
            size_t total = 0, used = 0;
            esp_err_t ret = esp_spiffs_info(NULL, &total, &used);
            if (ret == ESP_OK) ESP_LOGI(TAG, "SPIFFS registered, total size: %d, used: %d", total, used);

            // Default to SPIFFS
            _defaultToSPIFFS = true;
            _spiffsIsOk = true;
        }
    }

    // See if SD enabled
    _enableSD = fsConfig.getLong("sdEnabled", 0) != 0;
    bool _sdSPIMode = fsConfig.getLong("sdSPI", 0) != 0;
    // Init SD if enabled
    if (_enableSD) {
        ESP_LOGI(TAG, "setup SD mode %s", (_sdSPIMode ? "SDSPI" : "SDMMC"));

        esp_err_t ret;
        sdmmc_card_t* pCard;

        if (_sdSPIMode) {
            int sdMOSIPin = fsConfig.getLong("sdMOSI", -1);
            int sdMISOPin = fsConfig.getLong("sdMISO", -1);
            int sdCLKPin = fsConfig.getLong("sdCLK", -1);
            int sdCSPin = fsConfig.getLong("sdCS", -1);

            // Check valid
            if (sdMOSIPin == -1 || sdMISOPin == -1 || sdCLKPin == -1 || sdCSPin == -1) {
                ESP_LOGE(TAG, "setup SD bad pins");
                while (1) {
                }
            } else {
                ESP_LOGI(TAG, "setup SDSPI pins MOSI:%d MISO:%d CLK:%d CS:%d", sdMOSIPin, sdMISOPin, sdCLKPin, sdCSPin);
                esp_vfs_fat_sdmmc_mount_config_t mount_config = {
                    .format_if_mount_failed = false,
                    .max_files = 5,
                    .allocation_unit_size = 16 * 1024,
                };

                const char mount_point[] = "/sd";

                sdmmc_host_t host = SDSPI_HOST_DEFAULT();
                spi_bus_config_t bus_cfg = {
                    .mosi_io_num = sdMOSIPin,
                    .miso_io_num = sdMISOPin,
                    .sclk_io_num = sdCLKPin,
                    .quadwp_io_num = -1,
                    .quadhd_io_num = -1,
                    .max_transfer_sz = 4000,
                };
                ret = spi_bus_initialize(SPI3_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "SPI bus init error: %s", esp_err_to_name(ret));
                    esp_restart();
                }

                // This initializes the slot without card detect (CD) and write protect (WP) signals.
                // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
                sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
                slot_config.gpio_cs = (gpio_num_t)sdCSPin;
                slot_config.host_id = SPI3_HOST;

                ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &pCard);
            }
        } else {
            int sdLanes = fsConfig.getLong("sdLanes", 1);

            esp_vfs_fat_sdmmc_mount_config_t mount_config = {
                .format_if_mount_failed = true,
                .max_files = 5,
                .allocation_unit_size = 16 * 1024,
            };

            const char mount_point[] = "/sd";

            sdmmc_host_t host = SDMMC_HOST_DEFAULT();
            host.slot = 1;
            host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
            sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
            slot_config.width = sdLanes;
            slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

            // use internal pullups
            pinMode(15, INPUT_PULLUP);
            pinMode(2, INPUT_PULLUP);
            pinMode(4, INPUT_PULLUP);
            pinMode(12, INPUT_PULLUP);
            pinMode(13, INPUT_PULLUP);

            delay(200);

            ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &pCard);
        }

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            ESP_LOGE(TAG, "failed to init SD, error %s", esp_err_to_name(ret));
            ESP_LOGE(TAG, "!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        } else {
            _pSDCard = pCard;

            ESP_LOGI(TAG, "SD registered");

            // Default to SD
            _defaultToSPIFFS = false;
            _sdIsOk = true;

            // Check if we have a manifest file
            struct stat st;
            if (stat("/sd/manifest.json", &st) != 0) {
                ESP_LOGE(TAG, "creating empty manifest");
                FILE* pTmpFile = fopen("/sd/manifest.json", "w");
                const char* fileData = "{\"patterns\":[],\"playlists\":[]}";
                fwrite(fileData, 1, 30, pTmpFile);
                fclose(pTmpFile);
            } else {
                ESP_LOGI(TAG, "pattern manifest found");
            }
        }
    }

    if (!_spiffsIsOk || !_sdIsOk) {
        ESP_LOGE(TAG, "filesystems must both be marked online to ensure functionality...");
        ESP_LOGE(TAG, "SD: %s", (_sdIsOk ? "online" : "offline"));
        ESP_LOGE(TAG, "SPIFFS: %s", (_spiffsIsOk ? "online" : "offline"));
        if (!_spiffsIsOk) {
            // spiffs is MANDATORY, sd can wait for configuration
            esp_restart();
        }
    }
}

void FileManager::reformat(const String& fileSystemStr, String& respStr) {
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS)) {
        respStr = "{\"rslt\":\"fail\",\"error\":\"invalidfs\",\"files\":[]}";
        return;
    }

    // Reformat - need to disable Watchdog timer while formatting
    // Watchdog is not enabled on core 1 in Arduino according to this
    // https://www.bountysource.com/issues/44690700-watchdog-with-system-reset
    _cachedFileListValid = false;
    disableCore0WDT();
    esp_err_t ret = esp_spiffs_format(NULL);
    enableCore0WDT();
    Utils::setJsonBoolResult(respStr, ret == ESP_OK);
    ESP_LOGW(TAG, "reformat SPIFFS: %s", (ret == ESP_OK ? "OK" : "FAIL"));
}

bool FileManager::getFileInfo(const String& fileSystemStr, const String& filename, int& fileLength) {
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS)) {
        return false;
    }

    // Take mutex
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);

    // Check file exists
    struct stat st;
    String rootFilename = getFilePath(nameOfFS, filename);

    if (stat(rootFilename.c_str(), &st) != 0) {
        xSemaphoreGive(_fileSysMutex);
        return false;
    }
    if (!S_ISREG(st.st_mode)) {
        xSemaphoreGive(_fileSysMutex);
        return false;
    }
    xSemaphoreGive(_fileSysMutex);
    fileLength = st.st_size;
    return true;
}

bool FileManager::getFilesJSON(const String& fileSystemStr, const String& folderStr, String& respStr) {
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS)) {
        respStr = "{\"rslt\":\"fail\",\"error\":\"unknownfs\",\"files\":[]}";
        return false;
    }

    // Check if cached version can be used
    if ((_cachedFileListValid) && (_cachedFileListResponse.length() != 0)) {
        respStr = _cachedFileListResponse;
        return true;
    }
    // Take mutex
    if (xSemaphoreTake(_fileSysMutex, 0) != pdTRUE) {
        respStr = "{\"rslt\":\"fail\",\"error\":\"fsbusy\",\"files\":[]}";
        return false;
    }

    // Get size of file systems
    String baseFolderForFS;
    double fsSizeBytes = 0, fsUsedBytes = 0;
    if (nameOfFS == "spiffs") {
        uint32_t sizeBytes = 0, usedBytes = 0;
        esp_err_t ret = esp_spiffs_info(NULL, &sizeBytes, &usedBytes);
        if (ret != ESP_OK) {
            xSemaphoreGive(_fileSysMutex);
            respStr = "{\"rslt\":\"fail\",\"error\":\"SPIFFSINFO\",\"files\":[]}";
            return false;
        }
        // FS settings
        fsSizeBytes = sizeBytes;
        fsUsedBytes = usedBytes;
        nameOfFS = "spiffs";
        baseFolderForFS = "/spiffs";
    } else if (nameOfFS == "sd") {
        // Get size info
        sdmmc_card_t* pCard = (sdmmc_card_t*)_pSDCard;
        if (pCard) {
            fsSizeBytes = ((double)pCard->csd.capacity) * pCard->csd.sector_size;
            FATFS* fsinfo;
            DWORD fre_clust;
            if (f_getfree("0:", &fre_clust, &fsinfo) == 0) {
                fsUsedBytes = ((double)(fsinfo->csize)) * ((fsinfo->n_fatent - 2) - (fsinfo->free_clst))
#if _MAX_SS != 512
                              * (fsinfo->ssize);
#else
                              * 512;
#endif
            }
        }
        // Set FS info
        nameOfFS = "sd";
        baseFolderForFS = "/sd";
    }

    // Check file system is valid
    if (fsSizeBytes == 0) {
        ESP_LOGE(TAG, "getFilesJSON no valid FS");
        respStr = "{\"rslt\":\"fail\",\"error\":\"NOFS\",\"files\":[]}";
        return false;
    }

    // Open directory
    String rootFolder = (folderStr.startsWith("/") ? baseFolderForFS + folderStr : (baseFolderForFS + "/" + folderStr));
    DIR* dir = opendir(rootFolder.c_str());
    if (!dir) {
        xSemaphoreGive(_fileSysMutex);
        ESP_LOGE(TAG, "getFilesJSON failed to open base folder %s", rootFolder.c_str());
        respStr = "{\"rslt\":\"fail\",\"error\":\"nofolder\",\"files\":[]}";
        return false;
    }

    // Start response JSON
    respStr = "{\"rslt\":\"ok\",\"fsName\":\"" + nameOfFS + "\",\"fsBase\":\"" + baseFolderForFS + "\",\"diskSize\":" + String(fsSizeBytes) +
              ",\"diskUsed\":" + fsUsedBytes + ",\"folder\":\"" + String(rootFolder) + "\",\"files\":[";
    bool firstFile = true;

    // Read directory entries
    struct dirent* ent = NULL;
    while ((ent = readdir(dir)) != NULL) {
        // Check for unwanted files
        String fName = ent->d_name;
        if (fName.equalsIgnoreCase("System Volume Information")) continue;
        if (fName.equalsIgnoreCase("thumbs.db")) continue;
        if (fName.equalsIgnoreCase(".Trashes")) continue;
        if (fName.equalsIgnoreCase(".fseventsd")) continue;

        // Get file info including size
        size_t fileSize = 0;
        struct stat st;
        String filePath = (rootFolder.endsWith("/") ? rootFolder + fName : rootFolder + "/" + fName);
        if (stat(filePath.c_str(), &st) == 0) {
            fileSize = st.st_size;
        }

        // Form the JSON list
        if (!firstFile) respStr += ",";
        firstFile = false;
        respStr += "{\"name\":\"";
        respStr += ent->d_name;
        respStr += "\",\"size\":";
        respStr += String(fileSize);
        respStr += "}";
    }

    // Finished with file list
    closedir(dir);
    xSemaphoreGive(_fileSysMutex);

    // Complete string and replenish cache
    respStr += "]}";
    _cachedFileListResponse = respStr;
    _cachedFileListValid = true;
    return true;
}

String FileManager::getFileContents(const String& fileSystemStr, const String& filename, int maxLen) {
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS)) {
        return "";
    }

    // Take mutex
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);

    // Get file info - to check length
    String rootFilename = getFilePath(nameOfFS, filename);
    struct stat st;
    if (stat(rootFilename.c_str(), &st) != 0) {
        xSemaphoreGive(_fileSysMutex);
        return "";
    }
    if (!S_ISREG(st.st_mode)) {
        xSemaphoreGive(_fileSysMutex);
        return "";
    }

    // Check valid
    if (maxLen <= 0) {
        maxLen = ESP.getFreeHeap() / 3;
    }
    if (st.st_size >= maxLen - 1) {
        xSemaphoreGive(_fileSysMutex);
        return "";
    }
    int fileSize = st.st_size;

    // Open file
    FILE* pFile = fopen(rootFilename.c_str(), "rb");
    if (!pFile) {
        xSemaphoreGive(_fileSysMutex);
        return "";
    }

    // Buffer
    uint8_t* pBuf = new uint8_t[fileSize + 1];
    if (!pBuf) {
        fclose(pFile);
        xSemaphoreGive(_fileSysMutex);
        return "";
    }

    // Read
    size_t bytesRead = fread((char*)pBuf, 1, fileSize, pFile);
    fclose(pFile);
    xSemaphoreGive(_fileSysMutex);
    pBuf[bytesRead] = 0;
    String readData = (char*)pBuf;
    delete[] pBuf;
    return readData;
}

bool FileManager::setFileContents(const String& fileSystemStr, const String& filename, String& fileContents) {
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS)) {
        return false;
    }

    // Take mutex
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);

    // Open file for writing
    String rootFilename = getFilePath(nameOfFS, filename);
    FILE* pFile = fopen(rootFilename.c_str(), "wb");
    if (!pFile) {
        xSemaphoreGive(_fileSysMutex);
        return "";
    }

    // Write
    size_t bytesWritten = fwrite((uint8_t*)(fileContents.c_str()), 1, fileContents.length(), pFile);
    fclose(pFile);

    // Clean up
    _cachedFileListValid = false;
    xSemaphoreGive(_fileSysMutex);
    return bytesWritten == fileContents.length();
}

void FileManager::uploadAPIBlocksComplete() {
    // Cached file list now invalid
    _cachedFileListValid = false;
}

void FileManager::uploadAPIBlockHandler(const char* fileSystem, const String& req, const String& filename, int fileLength, size_t index,
                                        uint8_t* data, size_t len, bool finalBlock) {
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(String(fileSystem), nameOfFS)) return;

    // Take mutex
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);
    String tempFileName = "/__tmp__";
    String tmpRootFilename = getFilePath(nameOfFS, tempFileName);

    // Check if we should overwrite or append
    if (index == 0) {
        pChunkedFile = NULL;
        pChunkedFile = fopen(tmpRootFilename.c_str(), "wb");
    }

    if (!pChunkedFile) {
        xSemaphoreGive(_fileSysMutex);
        return;
    }

    // Write file block to temporary file
    size_t bytesWritten = fwrite(data, 1, len, pChunkedFile);

    // Rename if last block
    if (finalBlock) {
        fclose(pChunkedFile);
        pChunkedFile = NULL;
        // Check if destination file exists before renaming
        struct stat st;
        String rootFilename = getFilePath(nameOfFS, filename);
        if (stat(rootFilename.c_str(), &st) == 0) {
            // Remove in case filename already exists
            unlink(rootFilename.c_str());
        }

        // Rename
        rename(tmpRootFilename.c_str(), rootFilename.c_str());
    }

    // Restore semaphore
    xSemaphoreGive(_fileSysMutex);
}

bool FileManager::deleteFile(const String& fileSystemStr, const String& filename) {
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS)) {
        return false;
    }

    // Take mutex
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);

    // Remove file
    struct stat st;
    String rootFilename = getFilePath(nameOfFS, filename);
    if (stat(rootFilename.c_str(), &st) == 0) {
        unlink(rootFilename.c_str());
    }

    _cachedFileListValid = false;
    xSemaphoreGive(_fileSysMutex);
    return true;
}

bool FileManager::chunkedFileStart(const String& fileSystemStr, const String& filename, bool readByLine) {
    // Check file system supported
    String nameOfFS;
    if (!checkFileSystem(fileSystemStr, nameOfFS)) {
        return false;
    }

    // Take mutex
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);

    // Check file exists
    struct stat st;
    String rootFilename = getFilePath(nameOfFS, filename);
    if ((stat(rootFilename.c_str(), &st) != 0) || !S_ISREG(st.st_mode)) {
        xSemaphoreGive(_fileSysMutex);
        return false;
    }
    pChunkedFile = fopen(rootFilename.c_str(), "r");
    _chunkedFileLen = st.st_size;
    xSemaphoreGive(_fileSysMutex);

    // Setup access
    _chunkedFilename = rootFilename;
    _chunkedFileInProgress = true;
    _chunkedFilePos = 0;
    _chunkOnLineEndings = readByLine;

    return true;
}

char* FileManager::readLineFromFile(char* pBuf, int maxLen, FILE* pFile) {
    // Iterate over chars
    pBuf[0] = 0;
    char* pCurPtr = pBuf;
    int curLen = 0;
    while (true) {
        if (curLen >= maxLen - 1) break;
        int ch = fgetc(pFile);
        if (ch == EOF) {
            if (curLen != 0) break;
            return NULL;
        }
        if (ch == '\n') break;
        if (ch == '\r') continue;
        *pCurPtr++ = ch;
        *pCurPtr = 0;
        curLen++;
    }
    return pBuf;
}

uint8_t* FileManager::chunkFileNext(String& filename, int& fileLen, int& chunkPos, int& chunkLen, bool& finalChunk) {
    // Check valid
    chunkLen = 0;
    if (!_chunkedFileInProgress) return NULL;

    // Return details
    filename = _chunkedFilename;
    fileLen = _chunkedFileLen;
    chunkPos = _chunkedFilePos;

    // Take mutex
    xSemaphoreTake(_fileSysMutex, portMAX_DELAY);

    // Open file and seek
    FILE* pFile = NULL;
    if (_chunkOnLineEndings)
        pFile = fopen(_chunkedFilename.c_str(), "r");
    else
        pFile = fopen(_chunkedFilename.c_str(), "rb");
    if (!pFile) {
        xSemaphoreGive(_fileSysMutex);
        return NULL;
    }
    if ((_chunkedFilePos != 0) && (fseek(pFile, _chunkedFilePos, SEEK_SET) != 0)) {
        xSemaphoreGive(_fileSysMutex);
        fclose(pFile);
        return NULL;
    }

    // Handle data type
    if (_chunkOnLineEndings) {
        // Read a line
        char* pReadLine = readLineFromFile((char*)_chunkedFileBuffer, CHUNKED_BUF_MAXLEN - 1, pFile);
        // Ensure line is terminated
        if (!pReadLine) {
            finalChunk = true;
            _chunkedFileInProgress = false;
            chunkLen = 0;
        } else {
            chunkLen = strlen((char*)_chunkedFileBuffer);
        }
        // Record position
        _chunkedFilePos = ftell(pFile);
    } else {
        // Fill the buffer with file data
        chunkLen = fread((char*)_chunkedFileBuffer, 1, CHUNKED_BUF_MAXLEN, pFile);

        // Record position and check if this was the final block
        _chunkedFilePos = ftell(pFile);
        if ((chunkLen != CHUNKED_BUF_MAXLEN) || (_chunkedFileLen <= _chunkedFilePos)) {
            finalChunk = true;
            _chunkedFileInProgress = false;
        }
    }

    ESP_LOGV(TAG, "chunkNext filename %s chunklen %d filePos %d fileLen %d inprog %d final %d byLine %s\n", _chunkedFilename.c_str(),
             chunkLen, _chunkedFilePos, _chunkedFileLen, _chunkedFileInProgress, finalChunk, (_chunkOnLineEndings ? "Y" : "N"));

    // Close
    fclose(pFile);
    xSemaphoreGive(_fileSysMutex);
    return _chunkedFileBuffer;
}

// Get file name extension
String FileManager::getFileExtension(String& fileName) {
    String extn;
    // Find last .
    int dotPos = fileName.lastIndexOf('.');
    if (dotPos < 0) return extn;
    // Return substring
    return fileName.substring(dotPos + 1);
}

// Get file system and check ok
bool FileManager::checkFileSystem(const String& fileSystemStr, String& fsName) {
    // Check file system
    fsName = fileSystemStr;
    fsName.trim();
    fsName.toLowerCase();

    // Check for default
    if (fsName.length() == 0) {
        if (_defaultToSPIFFS)
            fsName = "spiffs";
        else
            fsName = "sd";
    }

    if (fsName == "spiffs") {
        if (!_spiffsIsOk) return false;
        return true;
    }
    if (fsName == "sd") {
        if (!_sdIsOk) return false;
        return true;
    }

    // Unknown FS
    return false;
}

String FileManager::getFilePath(const String& nameOfFS, const String& filename) {
    // Check if filename already contains file system
    if ((filename.indexOf("spiffs/") >= 0) || (filename.indexOf("sd/") >= 0)) return (filename.startsWith("/") ? filename : ("/" + filename));
    return (filename.startsWith("/") ? "/" + nameOfFS + filename : ("/" + nameOfFS + "/" + filename));
}
