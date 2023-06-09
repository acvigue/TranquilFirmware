// RBotFirmware
// Rob Dobson 2017

#pragma once

#include "FileManager.h"

class WorkManager;
class WorkItem;

class EvaluatorFiles
{
public:
    EvaluatorFiles(FileManager& fileManager, WorkManager& workManager);

    // Config
    void setConfig(const char* configStr);
    const char* getConfig();

    // Is Busy
    bool isBusy();

    //File name
    String fileName();

    //Total file length
    int getTotalFileLength();

    //Current file position
    int getCurrentFilePosition();

    //Current line length
    int getCurrentLineLength();
    
    // Check valid
    bool isValid(WorkItem& workItem);

    // Process WorkItem
    bool execWorkItem(WorkItem& workItem);

    // Call frequently
    void service();

    // Control
    void stop();

    // File types
    enum {
        FILE_TYPE_UNKNOWN,
        FILE_TYPE_GCODE,
        FILE_TYPE_THETA_RHO
    };
    
private:
    // Filename in progress
    bool _inProgress;

    // File manager & work manager
    FileManager& _fileManager;
    WorkManager& _workManager;

    // File type
    int _fileType;

    int _fileLen;
    int _filePos;
    int _chunkLen;

    String _fileName;

    // Start of file handling
    bool _firstValidLineProcessed;

    // Settings
    bool _interpolate;

private:
    int getFileTypeFromExtension(String& fileName);

};
