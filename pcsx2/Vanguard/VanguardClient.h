#pragma once
#include <string>

class VanguardClientUnmanaged
{
public:
    static void CORE_STEP();
    static void LOAD_GAME_START(std::string romPath);
    static void LOAD_GAME_DONE();
    static void LOAD_STATE_DONE();
    static void RESUME_EMULATION();
    static void GAME_CLOSED();
    static void CLOSE_GAME();
};
