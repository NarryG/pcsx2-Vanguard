#include "PrecompiledHeader.h"
#include "UnmanagedWrapper.h"
#include "App.h"
#include "AppSaveStates.h"
#include "GameDatabase.h"
#include "Memory.h"

//C++/CLI has various restrictions (no std::mutex for example), so we can't actually include certain headers directly
//What we CAN do is wrap those functions

void UnmanagedWrapper::VANGUARD_EXIT()
{
    wxGetApp().Exit();
}

void UnmanagedWrapper::VANGUARD_LOADSTATE(const wxString &file)
{
    StateCopy_LoadFromFile(file);
}

void UnmanagedWrapper::VANGUARD_SAVESTATE(const wxString &file)
{
    StateCopy_SaveToFile(file);
}

std::string UnmanagedWrapper::VANGUARD_GETGAMENAME()
{
    if (!curGameKey.IsEmpty()) {
        if (IGameDatabase *GameDB = AppHost_GetGameDatabase()) {
            Game_Data game;
            if (GameDB->findGame(game, curGameKey)) {
                return game.getString("Name").ToStdString();
            }
        }
    }
    return "UNKNOWNGAME";
}

void UnmanagedWrapper::EERAM_POKEBYTE(long long addr, unsigned char val)
{
    memWrite8(static_cast<u32>(addr), val);
}

unsigned char UnmanagedWrapper::EERAM_PEEKBYTE(long long addr)
{
    return memRead8(static_cast<u32>(addr));
}
