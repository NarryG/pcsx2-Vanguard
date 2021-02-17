#include "PrecompiledHeader.h"
#include "UnmanagedWrapper.h"
#include "App.h"
#include "AppSaveStates.h"
#include "GameDatabase.h"
#include "Memory.h"
#include "Cache.h"
#include "IopMem.h"
#include <Utilities/IniInterface.h>
#include "wx/sstream.h"
#include <GS.h>
#include <sstream>

//C++/CLI has various restrictions (no std::mutex for example), so we can't actually include certain headers directly
//What we CAN do is wrap those functions

void UnmanagedWrapper::VANGUARD_EXIT()
{
    wxGetApp().Exit();
}

void UnmanagedWrapper::VANGUARD_LOADSTATE(const wxString &file)
{
    StateCopy_LoadFromFile(file, false);
}

void UnmanagedWrapper::VANGUARD_SAVESTATE(const wxString &file)
{
    StateCopy_SaveToFile(file);
}

void UnmanagedWrapper::VANGUARD_RESUMEEMULATION()
{
    GetCoreThread().Resume();
}

void UnmanagedWrapper::VANGUARD_STOPGAME()
{
    UI_DisableSysShutdown();
    Console.SetTitle("PCSX2 Program Log");
    CoreThread.Reset();
}
void UnmanagedWrapper::VANGUARD_LOADGAME(const wxString &file)
{
    ScopedCoreThreadPause paused_core;
    SysUpdateIsoSrcFile(file);
    AppSaveSettings(); // save the new iso selection; update menus!
    sApp.SysExecute(g_Conf->CdvdSource, wxEmptyString);
}

std::string UnmanagedWrapper::VANGUARD_GETGAMENAME()
{

    const wxString newGameKey(SysGetDiscID());
    if (!newGameKey.IsEmpty()) {
        if (IGameDatabase *GameDB = AppHost_GetGameDatabase()) {
            Game_Data game;
            if (GameDB->findGame(game, newGameKey)) {
                return game.getString("Name").ToStdString();
            }
        }
    }
    //We can't find the name so return the id
    return newGameKey.ToStdString();
}

void UnmanagedWrapper::EERAM_POKEBYTE(long long addr, unsigned char val)
{
    memWrite8(static_cast<u32>(addr), val);
}

unsigned char UnmanagedWrapper::EERAM_PEEKBYTE(long long addr)
{
    return memRead8(static_cast<u32>(addr));
}
void UnmanagedWrapper::IOP_POKEBYTE(long long addr, unsigned char val)
{
    iopMemWrite8(static_cast<u32>(addr), val);
}

unsigned char UnmanagedWrapper::IOP_PEEKBYTE(long long addr)
{
    return iopMemRead8(static_cast<u32>(addr));
}
void UnmanagedWrapper::GS_POKEBYTE(long long addr, unsigned char val)
{
    gsWrite8(static_cast<u32>(addr), val);
}

unsigned char UnmanagedWrapper::GS_PEEKBYTE(long long addr)
{
    return gsRead8(static_cast<u32>(addr));
}
void UnmanagedWrapper::SPU_POKEBYTE(long long addr, unsigned char val)
{
    SPU2write(static_cast<u32>(addr), val);
}

unsigned char UnmanagedWrapper::SPU_PEEKBYTE(long long addr)
{
    u64 data = 0;
    //u8 byte = 0;
    data = SPU2read(static_cast<u32>(addr));/*
    std::stringstream stream;
    std::string alignment;
    stream << std::hex << addr;
    alignment = stream.str();
    alignment = alignment.back();
    if (alignment == "0" || alignment == "2" || alignment == "4" || alignment == "6" || alignment == "8" || alignment == "a" || alignment == "c" || alignment == "e")
        byte = data & 0xFF;
    if (alignment == "1" || alignment == "3" || alignment == "5" || alignment == "7" || alignment == "9" || alignment == "b" || alignment == "d" || alignment == "f")
        byte = (data >> 8) & 0xFF;*/
    return data;
}

std::string UnmanagedWrapper::VANGUARD_SAVECONFIG()
{

    auto a = new wxFileConfig();
    std::unique_ptr<wxFileConfig> vmini(a);
    IniSaver vmsaver(vmini.get());
    g_Conf->EmuOptions.LoadSave(vmsaver);
    sApp.DispatchVmSettingsEvent(vmsaver);

    wxStringOutputStream *pStrOutputStream = new wxStringOutputStream();
    a->Save(*pStrOutputStream);
    return pStrOutputStream->GetString().ToStdString();
}

void UnmanagedWrapper::VANGUARD_LOADCONFIG(wxString cfg)
{
    // Load virtual machine options and apply some defaults overtop saved items, which
    // are regulated by the PCSX2 UI.
    
    wxStringInputStream *pStrInStream = new wxStringInputStream(cfg);
    std::unique_ptr<wxFileConfig> vmini(new wxFileConfig(*pStrInStream));
    IniLoader vmloader(vmini.get());
    g_Conf->EmuOptions.LoadSave(vmloader);
    g_Conf->EmuOptions.GS.LimitScalar = g_Conf->Framerate.NominalScalar;

    if (g_Conf->EnablePresets) {
        g_Conf->IsOkApplyPreset(g_Conf->PresetIndex, true);
    }

    sApp.DispatchVmSettingsEvent(vmloader);
}