#pragma once
#include "includes.h"

#define LOG_MAX_LEN 256

/*  Singleton for the Logger    */

class Logger
{
private:
    static Logger* logger;

    void Init()
    {
    }
public:

    /* Static access method. */

    static Logger* Get();

public:

    NTSTATUS Start();

    void Log(const char* format, ...);

    /*	For some reason, #NPF handler will cause a CLOCK_WATCHDOG_VIOLATION if I don't log anything, 
        so I'll just create a seperate logging provider for all the spam.
    */

    void LogJunk(const char* format, ...);

    void End();
};