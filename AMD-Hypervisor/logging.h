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

    void End();
};