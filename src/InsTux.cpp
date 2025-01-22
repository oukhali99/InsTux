#include <fstream>
#include <iostream>
#include "interfaces.h"
#include "hooker.h"
#include "./Hooks/hooks.h"
#include "logger.h"
#include "Utils/netvarmanager.h"
#include "Utils/mousesim.h"
#include "fonts.h"
#include "settings.h"
#include <thread>

#ifdef USE_IMGUI
#include "gui/itgui.h"
#endif

std::thread* startThread;

void Start() {
    //InitLogger();

    //CreateMaterialFile();

    Interfaces::FindInterfaces();
    Interfaces::DumpInterfaces();

    Msg("++++ InsTux starting... \n");

    Hooker::FindIClientMode();
    //Hooker::FindSendPacket();

    Hooker::InitializeVMHooks();

    modelRenderVMT->HookVM((void*) Hooks::DrawModelExecute, DRAW_MODEL_EXECUTE_IDX);
    modelRenderVMT->ApplyVMT();

    //clientModeVMT->HookVM((void*) Hooks::CreateMove, CREATE_MOVE_IDX);
    //clientModeVMT->ApplyVMT();

    engineVGuiVMT->HookVM((void*) Hooks::Paint, 15);
    engineVGuiVMT->ApplyVMT();

    NetVarManager::DumpNetvars();
    Offsets::GetOffsets();
    Fonts::SetupFonts();

    MouseSim::mouseInit();

    Settings::LoadConfig();

#ifdef USE_IMGUI
    GUI::Init();
#endif
    
    Msg("++++ InsTux loading complete! ++++\n");
}

// Called on library load
int __attribute__((constructor)) instux_init()
{
    startThread = new std::thread(Start);
    return 0;
}

// Called on library unload
void __attribute__((destructor)) instux_shutdown()
{
#ifdef USE_IMGUI
    GUI::DeInit();
#endif

    startThread->join();
    delete startThread;

    MouseSim::mouseDestroy();
    modelRenderVMT->ReleaseVMT();
    clientModeVMT->ReleaseVMT();
    engineVGuiVMT->ReleaseVMT();
}
