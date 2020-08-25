// A basic test implementation of Netcore for IPC in Dolphin

#pragma warning(disable : 4564)
#include <string>

#include "Helpers.hpp"
#include "VanguardClient.h"
#include "VanguardClientInitializer.h"
#include "PCSX2MemoryDomain.h"

#include <msclr/marshal_cppstd.h>
#include "UnmanagedWrapper.h"
#include "wx/string.h"

typedef char s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef uintptr_t uptr;
typedef intptr_t sptr;

typedef unsigned int uint;

using namespace cli;
using namespace System;
using namespace Text;
using namespace RTCV;
using namespace RTCV::CorruptCore::Extensions;
using namespace NetCore;
using namespace CorruptCore;
using namespace Vanguard;
using namespace Runtime::InteropServices;
using namespace Threading;
using namespace Collections::Generic;
using namespace Reflection;
using namespace Diagnostics;

#using < system.dll >
#using < system.windows.forms.dll >


// Define this in here as it's managed and weird stuff happens if it's in a header
public
ref class VanguardClient
{
public:
    static NetCoreReceiver ^ receiver;
    static VanguardConnector ^ connector;

    static void OnMessageReceived(Object ^ sender, NetCoreEventArgs ^ e);
    static void SpecUpdated(Object ^ sender, SpecUpdateEventArgs ^ e);
    static void RegisterVanguardSpec();

    static void StartClient();
    static void RestartClient();
    static void StopClient();

    static void LoadRom(String ^ filename);
    static bool LoadState(std::string filename);
    static bool SaveState(String ^ filename);

    //String ^ GetConfigAsJson(VanguardSettingsWrapper ^ settings);
    //VanguardSettingsWrapper ^ GetConfigFromJson(String ^ json);

    static String ^ emuDir = IO::Path::GetDirectoryName(Assembly::GetExecutingAssembly()->Location);
    static String ^ logPath = IO::Path::Combine(emuDir, "EMU_LOG.txt");

    static array<String ^> ^ configPaths;

    static volatile bool gameLoading = false;
    static volatile bool stateLoading = false;
    static bool attached = false;
    static bool enableRTC = true;
};


static PartialSpec ^ getDefaultPartial() {
    PartialSpec ^ partial = gcnew PartialSpec("VanguardSpec");
    partial->Set(VSPEC::NAME, "PCSX2");
    partial->Set(VSPEC::SUPPORTS_RENDERING, false);
    partial->Set(VSPEC::SUPPORTS_CONFIG_MANAGEMENT, false);
    partial->Set(VSPEC::SUPPORTS_CONFIG_HANDOFF, false);
    partial->Set(VSPEC::SUPPORTS_KILLSWITCH, true);
    partial->Set(VSPEC::SUPPORTS_REALTIME, true);
    partial->Set(VSPEC::SUPPORTS_SAVESTATES, true);
    partial->Set(VSPEC::SUPPORTS_REFERENCES, true);
    partial->Set(VSPEC::SUPPORTS_MIXED_STOCKPILE, true);
    partial->Set(VSPEC::CONFIG_PATHS, VanguardClient::configPaths);
    partial->Set(VSPEC::SYSTEM, "PCSX2");
    partial->Set(VSPEC::GAMENAME, String::Empty);
    partial->Set(VSPEC::SYSTEMPREFIX, String::Empty);
    partial->Set(VSPEC::OPENROMFILENAME, String::Empty);
    partial->Set(VSPEC::OVERRIDE_DEFAULTMAXINTENSITY, 500000);
    partial->Set(VSPEC::SYNCSETTINGS, String::Empty);
    partial->Set(VSPEC::MEMORYDOMAINS_BLACKLISTEDDOMAINS, gcnew array<String ^>{});
    partial->Set(VSPEC::LOADSTATE_USES_CALLBACKS, true);
    partial->Set(VSPEC::SYSTEM, String::Empty);
    partial->Set(VSPEC::EMUDIR, VanguardClient::emuDir);

    return partial;
}

    void VanguardClient::SpecUpdated(Object ^ sender, SpecUpdateEventArgs ^ e)
{
    PartialSpec ^ partial = e->partialSpec;

    LocalNetCoreRouter::Route(NetcoreCommands::CORRUPTCORE,
                              NetcoreCommands::REMOTE_PUSHVANGUARDSPECUPDATE, partial, true);
    LocalNetCoreRouter::Route(NetcoreCommands::UI, NetcoreCommands::REMOTE_PUSHVANGUARDSPECUPDATE,
                              partial, true);
}


void VanguardClient::RegisterVanguardSpec()
{
    PartialSpec ^ emuSpecTemplate = gcnew PartialSpec("VanguardSpec");

    emuSpecTemplate->Insert(getDefaultPartial());

    AllSpec::VanguardSpec = gcnew FullSpec(emuSpecTemplate, true);
    // You have to feed a partial spec as a template

    if (attached)
        VanguardConnector::PushVanguardSpecRef(AllSpec::VanguardSpec);

    LocalNetCoreRouter::Route(NetcoreCommands::CORRUPTCORE, NetcoreCommands::REMOTE_PUSHVANGUARDSPEC, emuSpecTemplate, true);
    LocalNetCoreRouter::Route(NetcoreCommands::UI, NetcoreCommands::REMOTE_PUSHVANGUARDSPEC, emuSpecTemplate, true);
    AllSpec::VanguardSpec->SpecUpdated += gcnew EventHandler<SpecUpdateEventArgs ^>(&VanguardClient::SpecUpdated);
}

// Lifted from Bizhawk
static Assembly ^ CurrentDomain_AssemblyResolve(Object ^ sender, ResolveEventArgs ^ args) {
    try {
        Trace::WriteLine("Entering AssemblyResolve\n" + args->Name + "\n" +
                         args->RequestingAssembly);
        String ^ requested = args->Name;
        Monitor::Enter(AppDomain::CurrentDomain);
        {
            array<Assembly ^> ^ asms = AppDomain::CurrentDomain->GetAssemblies();
            for (int i = 0; i < asms->Length; i++) {
                Assembly ^ a = asms[i];
                if (a->FullName == requested) {
                    return a;
                }
            }

            AssemblyName ^ n = gcnew AssemblyName(requested);
            // load missing assemblies by trying to find them in the dll directory
            String ^ dllname = n->Name + ".dll";
            String ^ directory = IO::Path::Combine(
                IO::Path::GetDirectoryName(Assembly::GetExecutingAssembly()->Location), "..", "RTCV");
            String ^ fname = IO::Path::Combine(directory, dllname);
            if (!IO::File::Exists(fname)) {
                Trace::WriteLine(fname + " doesn't exist");
                return nullptr;
            }

            // it is important that we use LoadFile here and not load from a byte array; otherwise
            // mixed (managed/unamanged) assemblies can't load
            Trace::WriteLine("Loading " + fname);
            return Assembly::UnsafeLoadFrom(fname);
        }
    } catch (Exception ^ e) {
        Trace::WriteLine("Something went really wrong in AssemblyResolve. Send this to the devs\n" +
                         e);
        return nullptr;
    } finally {
        Monitor::Exit(AppDomain::CurrentDomain);
    }
}

// Create our VanguardClient
void VanguardClientInitializer::StartVanguardClient()
{

    // this needs to be done before the warnings/errors show up
    System::Windows::Forms::Application::EnableVisualStyles();
    System::Windows::Forms::Application::SetCompatibleTextRenderingDefault(false);

    System::Windows::Forms::Form ^ dummy = gcnew System::Windows::Forms::Form();
    IntPtr Handle = dummy->Handle;
    SyncObjectSingleton::SyncObject = dummy;
    SyncObjectSingleton::UseQueue = true;

    //Todo
    VanguardClient::configPaths = gcnew array<String ^>{
        IO::Path::Combine(VanguardClient::emuDir, "inis", "FWnull.ini"),
        IO::Path::Combine(VanguardClient::emuDir, "inis", "GSdx.ini"),
        IO::Path::Combine(VanguardClient::emuDir, "inis", "PCSX2_vm.ini"),
        IO::Path::Combine(VanguardClient::emuDir, "inis", "WiimoteNew.ini"),
        IO::Path::Combine(VanguardClient::emuDir, "inis", "SPU2-X.ini")};

    VanguardClient::StartClient();
    VanguardClient::RegisterVanguardSpec();
    RtcCore::StartEmuSide();

    //Lie if we're in attached
    if (VanguardClient::attached)
        VanguardConnector::ImplyClientConnected();
}

// Create our VanguardClient
void VanguardClientInitializer::Initialize()
{
    // This has to be in its own method where no other dlls are used so the JIT can compile it
    AppDomain::CurrentDomain->AssemblyResolve +=
        gcnew ResolveEventHandler(CurrentDomain_AssemblyResolve);

    StartVanguardClient();
}

void VanguardClient::StartClient()
{
    RTCV::Common::Logging::StartLogging(logPath);
    // Can't use contains
    auto args = Environment::GetCommandLineArgs();
    for (int i = 0; i < args->Length; i++) {
        if (args[i] == "-CONSOLE") {
        }
        if (args[i] == "-ATTACHED") {
            attached = true;
        }
        if (args[i] == "-DISABLERTC") {
            enableRTC = false;
        }
    }

    receiver = gcnew NetCoreReceiver();
    receiver->Attached = attached;
    receiver->MessageReceived += gcnew EventHandler<NetCoreEventArgs ^>(&VanguardClient::OnMessageReceived);
    connector = gcnew VanguardConnector(receiver);
}

void VanguardClient::RestartClient()
{
    connector->Kill();
    connector = nullptr;
    StartClient();
}

void VanguardClient::StopClient()
{
    connector->Kill();
    connector = nullptr;
}

#pragma region MemoryDomains
static array<MemoryDomainProxy ^> ^ GetInterfaces() {
    if (String::IsNullOrWhiteSpace(AllSpec::VanguardSpec->Get<String ^>(VSPEC::OPENROMFILENAME)))
        return gcnew array<MemoryDomainProxy ^>(0);

    array<MemoryDomainProxy ^> ^ interfaces = gcnew array<MemoryDomainProxy ^>(1);
    interfaces[0] = gcnew MemoryDomainProxy(gcnew EERAM);

    return interfaces;
}

    static bool RefreshDomains(bool updateSpecs = true)
{
    array<MemoryDomainProxy ^> ^ oldInterfaces =
        AllSpec::VanguardSpec->Get<array<MemoryDomainProxy ^> ^>(VSPEC::MEMORYDOMAINS_INTERFACES);
    array<MemoryDomainProxy ^> ^ newInterfaces = GetInterfaces();

    // Bruteforce it since domains can change inconsistently in some configs and we keep code
    // consistent between implementations
    bool domainsChanged = false;
    if (oldInterfaces == nullptr)
        domainsChanged = true;
    else {
        domainsChanged = oldInterfaces->Length != newInterfaces->Length;
        for (int i = 0; i < oldInterfaces->Length; i++) {
            if (domainsChanged)
                break;
            if (oldInterfaces[i]->Name != newInterfaces[i]->Name)
                domainsChanged = true;
            if (oldInterfaces[i]->Size != newInterfaces[i]->Size)
                domainsChanged = true;
        }
    }

    if (updateSpecs) {
        AllSpec::VanguardSpec->Update(VSPEC::MEMORYDOMAINS_INTERFACES, newInterfaces, true, true);
        LocalNetCoreRouter::Route(NetcoreCommands::CORRUPTCORE,
                                  NetcoreCommands::REMOTE_EVENT_DOMAINSUPDATED, domainsChanged, true);
    }

    return domainsChanged;
}


#pragma endregion

//Todo
#pragma region Settings
/*
	String ^ VanguardClient::GetConfigAsJson(VanguardSettingsWrapper ^ settings)
{
    return JsonHelper::Serialize(settings);
}

VanguardSettingsWrapper ^ VanguardClient::GetConfigFromJson(String ^ str)
{
    return JsonHelper::Deserialize<VanguardSettingsWrapper ^>(str);
}
*/
#pragma endregion
static void STEP_CORRUPT() // errors trapped by CPU_STEP
{
    RtcClock::STEP_CORRUPT(true, true);
}

#pragma region Hooks
void VanguardClientUnmanaged::CORE_STEP()
{
    if (!VanguardClient::enableRTC)
        return;
    // Any step hook for corruption
    ActionDistributor::Execute("ACTION");
    STEP_CORRUPT();
}

// This is on the main thread not the emu thread
void VanguardClientUnmanaged::LOAD_GAME_START(std::string romPath)
{
    if (!VanguardClient::enableRTC)
        return;
    StepActions::ClearStepBlastUnits();

    RtcClock::RESET_COUNT();

    String ^ gameName = Helpers::utf8StringToSystemString(romPath);
    AllSpec::VanguardSpec->Update(VSPEC::OPENROMFILENAME, gameName, true, true);
}

void VanguardClientUnmanaged::LOAD_GAME_DONE()
{
    if (!VanguardClient::enableRTC)
        return;
    PartialSpec ^ gameDone = gcnew PartialSpec("VanguardSpec");

    try {
        gameDone->Set(VSPEC::SYSTEM, "PCSX2");
        gameDone->Set(VSPEC::SYSTEMPREFIX, "PCSX2");
        gameDone->Set(VSPEC::SYSTEMCORE, "PS2");
        gameDone->Set(VSPEC::SYNCSETTINGS, "");
        gameDone->Set(VSPEC::MEMORYDOMAINS_BLACKLISTEDDOMAINS, gcnew array<String ^>{});
        gameDone->Set(VSPEC::CORE_DISKBASED, true);

        String ^ oldGame = AllSpec::VanguardSpec->Get<String ^>(VSPEC::GAMENAME);

        String ^ gameName = Helpers::utf8StringToSystemString(UnmanagedWrapper::VANGUARD_GETGAMENAME());

        char replaceChar = L'-';
        gameDone->Set(VSPEC::GAMENAME, StringExtensions::MakeSafeFilename(gameName, replaceChar));



        String ^ syncsettings = Helpers::utf8StringToSystemString(UnmanagedWrapper::VANGUARD_SAVECONFIG());
        gameDone->Set(VSPEC::SYNCSETTINGS, syncsettings);

        AllSpec::VanguardSpec->Update(gameDone, true, false);

        bool domainsChanged = RefreshDomains(true);
        if (oldGame != gameName) {
            LocalNetCoreRouter::Route(NetcoreCommands::UI, NetcoreCommands::RESET_GAME_PROTECTION_IF_RUNNING, true);
        }
    } catch (Exception ^ e) {
        Trace::WriteLine(e->ToString());
    }
    VanguardClient::gameLoading = false;
    RtcCore::LOAD_GAME_DONE();
}

void VanguardClientUnmanaged::LOAD_STATE_DONE()
{
    if (!VanguardClient::enableRTC)
        return;
    VanguardClient::stateLoading = false;
}

void VanguardClientUnmanaged::GAME_CLOSED()
{
    if (!VanguardClient::enableRTC)
        return;
    AllSpec::VanguardSpec->Update(VSPEC::OPENROMFILENAME, "", true, false);
    RefreshDomains();
    RtcCore::GAME_CLOSED(true);
}


bool VanguardClientUnmanaged::RTC_OSD_ENABLED()
{
    if (!VanguardClient::enableRTC)
        return true;
    if (RTCV::NetCore::Params::IsParamSet(RTCSPEC::CORE_EMULATOROSDDISABLED))
        return false;
    return true;
}
#pragma endregion

/*ENUMS FOR THE SWITCH STATEMENT*/
enum COMMANDS {
    SAVESAVESTATE,
    LOADSAVESTATE,
    REMOTE_LOADROM,
    REMOTE_CLOSEGAME,
    REMOTE_DOMAIN_GETDOMAINS,
    REMOTE_KEY_SETSYNCSETTINGS,
    REMOTE_KEY_SETSYSTEMCORE,
    REMOTE_EVENT_EMU_MAINFORM_CLOSE,
    REMOTE_EVENT_EMUSTARTED,
    REMOTE_ISNORMALADVANCE,
    REMOTE_EVENT_CLOSEEMULATOR,
    REMOTE_ALLSPECSSENT,
    REMOTE_RESUMEEMULATION,
    UNKNOWN
};

inline COMMANDS CheckCommand(String ^ inString)
{
    if (inString == "LOADSAVESTATE")
        return LOADSAVESTATE;
    if (inString == "SAVESAVESTATE")
        return SAVESAVESTATE;
    if (inString == "REMOTE_LOADROM")
        return REMOTE_LOADROM;
    if (inString == "REMOTE_CLOSEGAME")
        return REMOTE_CLOSEGAME;
    if (inString == "REMOTE_ALLSPECSSENT")
        return REMOTE_ALLSPECSSENT;
    if (inString == "REMOTE_DOMAIN_GETDOMAINS")
        return REMOTE_DOMAIN_GETDOMAINS;
    if (inString == "REMOTE_KEY_SETSYSTEMCORE")
        return REMOTE_KEY_SETSYSTEMCORE;
    if (inString == "REMOTE_KEY_SETSYNCSETTINGS")
        return REMOTE_KEY_SETSYNCSETTINGS;
    if (inString == "REMOTE_EVENT_EMU_MAINFORM_CLOSE")
        return REMOTE_EVENT_EMU_MAINFORM_CLOSE;
    if (inString == "REMOTE_EVENT_EMUSTARTED")
        return REMOTE_EVENT_EMUSTARTED;
    if (inString == "REMOTE_ISNORMALADVANCE")
        return REMOTE_ISNORMALADVANCE;
    if (inString == "REMOTE_EVENT_CLOSEEMULATOR")
        return REMOTE_EVENT_CLOSEEMULATOR;
    if (inString == "REMOTE_ALLSPECSSENT")
        return REMOTE_ALLSPECSSENT;
    if (inString == "REMOTE_RESUMEEMULATION")
        return REMOTE_RESUMEEMULATION;
    return UNKNOWN;
}

/* IMPLEMENT YOUR COMMANDS HERE */

void VanguardClient::LoadRom(String ^ filename)
{
    String ^ currentOpenRom = "";
    if (AllSpec::VanguardSpec->Get<String ^>(VSPEC::OPENROMFILENAME) != "")
        currentOpenRom = AllSpec::VanguardSpec->Get<String ^>(VSPEC::OPENROMFILENAME);

    // Game is not running
    if (currentOpenRom != filename) {
        // Clear out any old settings
        //Config::ClearCurrentVanguardLayer();

        const std::string &path = Helpers::systemStringToUtf8String(filename);
        gameLoading = true;
        UnmanagedWrapper::VANGUARD_LOADGAME(wxString(path));

        // We have to do it this way to prevent deadlock due to synced calls. It sucks but it's required at the moment
        int i = 0;
        while (gameLoading) {
            Thread::Sleep(20);
            System::Windows::Forms::Application::DoEvents();

            //We wait for 20 ms every time. If loading a game takes longer than 15 seconds, break out.
            if (++i > 750)
                gameLoading = false;
        }
        Thread::Sleep(100); // Give the emu thread a chance to recover
    }
    return;
}

bool VanguardClient::LoadState(std::string filename)
{
    StepActions::ClearStepBlastUnits();
    RtcClock::RESET_COUNT();
    wxString mystring(filename);
    stateLoading = true;
    UnmanagedWrapper::VANGUARD_LOADSTATE(mystring);
    // We have to do it this way to prevent deadlock due to synced calls. It sucks but it's required at the moment
    int i = 0;
    do {
        Thread::Sleep(20);
        System::Windows::Forms::Application::DoEvents();

        //We wait for 20 ms every time. If loading a game takes longer than 10 seconds, break out.
        if (++i > 500){
            stateLoading = false;
            return false;
        }
    } while (stateLoading);
    return true;
}

bool VanguardClient::SaveState(String ^ filename)
{
    std::string converted_filename = Helpers::systemStringToUtf8String(filename);
    wxString mystring(converted_filename);
    UnmanagedWrapper::VANGUARD_SAVESTATE(mystring);
    Thread::Sleep(100);
    return true;
}

// No fun anonymous classes with closure here
#pragma region Delegates
void StopGame() //Todo
{
    UnmanagedWrapper::VANGUARD_STOPGAME();
    Thread::Sleep(100);
}

void Quit()
{
    Environment::Exit(-1);
    //UnmanagedWrapper::VANGUARD_EXIT();
}

void AllSpecsSent()
{
}
#pragma endregion

/* THIS IS WHERE YOU HANDLE ANY RECEIVED MESSAGES */
void VanguardClient::OnMessageReceived(Object ^ sender, NetCoreEventArgs ^ e)
{
    if (!VanguardClient::enableRTC)
        return;

    NetCoreMessage ^ message = e->message;
    NetCoreSimpleMessage ^ simpleMessage;
    NetCoreAdvancedMessage ^ advancedMessage;

    if (Helpers::is<NetCoreSimpleMessage ^>(message))
        simpleMessage = static_cast<NetCoreSimpleMessage ^>(message);
    if (Helpers::is<NetCoreAdvancedMessage ^>(message))
        advancedMessage = static_cast<NetCoreAdvancedMessage ^>(message);

    switch (CheckCommand(message->Type)) {
        case REMOTE_ALLSPECSSENT: {
            SyncObjectSingleton::GenericDelegate ^ g =
                gcnew SyncObjectSingleton::GenericDelegate(&AllSpecsSent);
            SyncObjectSingleton::FormExecute(g);
        } break;

        case LOADSAVESTATE: {
            array<Object ^> ^ cmd = static_cast<array<Object ^> ^>(advancedMessage->objectValue);
            String ^ path = static_cast<String ^>(cmd[0]);
            std::string converted_path = Helpers::systemStringToUtf8String(path);
            StashKeySavestateLocation ^ location = safe_cast<StashKeySavestateLocation ^>(cmd[1]);

            // Clear out any old settings
            // Config::ClearCurrentVanguardLayer();

            // Load up the sync settings //Todo
            String ^ settingStr = AllSpec::VanguardSpec->Get<String ^>(VSPEC::SYNCSETTINGS);
            if (!String::IsNullOrWhiteSpace(settingStr)) {
                std::string ss = Helpers::systemStringToUtf8String(settingStr);
                UnmanagedWrapper::VANGUARD_LOADCONFIG(wxString(ss));
            }
            e->setReturnValue(LoadState(converted_path));
        } break;

        case SAVESAVESTATE: {
            String ^ Key = (String ^)(advancedMessage->objectValue);
            // Build the shortname
            String ^ quickSlotName = Key + ".timejump";

            // Get the prefix for the state

            String ^ gameName = Helpers::utf8StringToSystemString(UnmanagedWrapper::VANGUARD_GETGAMENAME());

            char replaceChar = L'-';
            String ^ prefix = StringExtensions::MakeSafeFilename(gameName, replaceChar);
            prefix = prefix->Substring(prefix->LastIndexOf('\\') + 1);

            // Build up our path
            String ^ path = RtcCore::workingDir + IO::Path::DirectorySeparatorChar +
                            "SESSION" + IO::Path::DirectorySeparatorChar + prefix + "." + quickSlotName + ".State";

            // If the path doesn't exist, make it
            IO::FileInfo ^ file = gcnew IO::FileInfo(path);
            if (file->Directory != nullptr && file->Directory->Exists == false)
                file->Directory->Create();

            if (SaveState(path))
                e->setReturnValue(path);
        } break;

        case REMOTE_LOADROM: {
            String ^ filename = (String ^) advancedMessage->objectValue;

            System::Action<String ^> ^ a = gcnew Action<String ^>(&LoadRom);
            SyncObjectSingleton::FormExecute<String ^>(a, filename);
        } break;

        case REMOTE_CLOSEGAME: {
            SyncObjectSingleton::GenericDelegate ^ g =
                gcnew SyncObjectSingleton::GenericDelegate(&StopGame);
            SyncObjectSingleton::FormExecute(g);
        } break;

        case REMOTE_DOMAIN_GETDOMAINS: {
            RefreshDomains();
        } break;

        case REMOTE_KEY_SETSYNCSETTINGS: {
            String ^ settings = (String ^)(advancedMessage->objectValue);
            AllSpec::VanguardSpec->Set(VSPEC::SYNCSETTINGS, settings);
        } break;

        case REMOTE_KEY_SETSYSTEMCORE: {
            // Do nothing
        } break;

        case REMOTE_EVENT_EMUSTARTED: {
        } break;

        case REMOTE_ISNORMALADVANCE: {
            // Todo - Dig out fast forward?
            e->setReturnValue(true);
        } break;

        case REMOTE_EVENT_EMU_MAINFORM_CLOSE:
        case REMOTE_EVENT_CLOSEEMULATOR: {
            SyncObjectSingleton::GenericDelegate ^ g = gcnew SyncObjectSingleton::GenericDelegate(&StopGame);
            SyncObjectSingleton::FormExecute(g);
            Thread::Sleep(50);
            StopClient();
            Thread::Sleep(50);
            g = gcnew SyncObjectSingleton::GenericDelegate(&Quit);
            SyncObjectSingleton::FormExecute(g);
        } break;

        case REMOTE_RESUMEEMULATION: {
            UnmanagedWrapper::VANGUARD_RESUMEEMULATION();
        } break;

        default:
            break;
    }
}
