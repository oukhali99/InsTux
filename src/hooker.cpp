#include "hooker.h"
#include "logger.h"

bool* bSendPacket = nullptr;
int* nPredictionRandomSeed = nullptr;
CMoveData* g_MoveData = nullptr;
uint8_t* CrosshairWeaponTypeCheck = nullptr;
uint8_t* CamThinkSvCheatsCheck = nullptr;

VMT* modelRenderVMT = nullptr;
VMT* clientModeVMT = nullptr;
VMT* renderViewVMT = nullptr;

VMT* materialVMT = nullptr;

VMT* surfaceVMT = nullptr;
VMT* engineVGuiVMT = nullptr;

uintptr_t* GetCSWpnData_address = nullptr;

uintptr_t oSwapWindow;
uintptr_t* swapWindowJumpAddress = nullptr;

uintptr_t oPollEvent;
uintptr_t* polleventJumpAddress = nullptr;

MsgFunc_ServerRankRevealAllFn MsgFunc_ServerRankRevealAll;
SendClanTagFn SendClanTag;
IsReadyCallbackFn IsReadyCallback;

RecvVarProxyFn fnSequenceProxyFn;

StartDrawingFn StartDrawing;
FinishDrawingFn FinishDrawing;

GetLocalClientFn GetLocalClient;

InitKeyValuesFn InitKeyValues;
LoadFromBufferFn LoadFromBuffer;

std::vector<dlinfo_t> libraries;

// taken form aixxe's cstrike-basehook-linux
bool Hooker::GetLibraryInformation(const char* library, uintptr_t* address, size_t* size) {
    if (libraries.size() == 0) {
        dl_iterate_phdr([] (struct dl_phdr_info* info, size_t, void*) {
            dlinfo_t library_info = {};

            library_info.library = info->dlpi_name;
            library_info.address = info->dlpi_addr + info->dlpi_phdr[0].p_vaddr;
            library_info.size = info->dlpi_phdr[0].p_memsz;

            libraries.push_back(library_info);

            return 0;
        }, nullptr);
    }

    for (const dlinfo_t& current: libraries) {
        if (!strcasestr(current.library, library))
            continue;

        if (address)
            *address = current.address;

        if (size)
            *size = current.size;

        return true;
    }

    return false;
}

// taken form aixxe's cstrike-basehook-linux
uintptr_t GetLibraryBase(const char* library) {
    if (libraries.size() == 0) {
        dl_iterate_phdr([] (struct dl_phdr_info* info, size_t, void*) {
            dlinfo_t library_info = {};

            library_info.library = info->dlpi_name;
            library_info.address = info->dlpi_addr;

            libraries.push_back(library_info);

            return 0;
        }, nullptr);
    }

    for (const dlinfo_t& current: libraries) {
        //Log << "        -- " << current.library << std::endl;
        if (strcasestr(current.library, library))
            return current.address;
    }

    //Log << "    -- Failed to GetLibraryBase for " << library << std::endl;
    return 0;
}

void Hooker::InitializeVMHooks()
{
    modelRenderVMT = new VMT(modelRender);
    surfaceVMT = new VMT(surface);
    engineVGuiVMT = new VMT(engineVGui);
    renderViewVMT = new VMT(renderView);

    /*
    gameEventsVMT = new VMT(gameEvents);
    viewRenderVMT = new VMT(viewRender);
    inputInternalVMT = new VMT(inputInternal);
    launcherMgrVMT = new VMT(launcherMgr);
    soundVMT = new VMT(sound);
    */
}

void Hooker::FindInitKeyValues()
{
    //uintptr_t func_addr = PatternFinder::FindPatternInModule("bin/client.so", (unsigned char*) INITKEYVALUES_SIGNATURE, INITKEYVALUES_MASK);
    unsigned func_addr = GetLibraryBase("bin/client.so");
    //Log << "++ client.so base = " << hex((unsigned)func_addr) << std::endl;

    func_addr = (unsigned)func_addr + (unsigned) 0x949420;
    InitKeyValues = reinterpret_cast<InitKeyValuesFn>(func_addr);
    //Log << "+ InitKeyValues = " << hex0((unsigned)InitKeyValues) << std::endl;
}

void Hooker::FindLoadFromBuffer()
{
    //uintptr_t func_addr = PatternFinder::FindPatternInModule("bin/client.so", (unsigned char*) LOADFROMBUFFER_SIGNATURE, LOADFROMBUFFER_MASK);
    unsigned func_addr = GetLibraryBase("bin/client.so");
    //Log << "++ client.so base = " << hex((unsigned)func_addr) << std::endl;
    func_addr = (unsigned)func_addr + (unsigned) 0x94E840;

    LoadFromBuffer = reinterpret_cast<LoadFromBufferFn>(func_addr);
    //Log << "+ LoadFromBuffer = " << hex0((unsigned)LoadFromBuffer) << std::endl;
}

void Hooker::FindIClientMode()
{
    //GetClientModeFn GetClientMode = reinterpret_cast<GetClientModeFn> (GetLibraryBase("bin/client.so") + 0x705300);
    uintptr_t func_ptr = PatternFinder::FindPatternInModule("bin/client.so", (unsigned char*) ICLIENTMODE_SIG, ICLIENTMODE_MASK) + strlen(ICLIENTMODE_MASK);

    GetClientModeFn GetClientMode = reinterpret_cast<GetClientModeFn> (GetAbsoluteAddress(func_ptr, 0, 4));

    clientMode = GetClientMode();
    //Log << "* GetClientMode returned " << hex0((unsigned)clientMode) << std::endl;

    clientModeVMT = new VMT(clientMode);
}

void Hooker::FindSendPacket()
{
    uintptr_t bool_address = PatternFinder::FindPatternInModule("engine.so", (unsigned char*) BSENDPACKET_SIGNATURE, BSENDPACKET_MASK) + strlen(BSENDPACKET_MASK);

    //Log << "  bool_address = " << hex0(bool_address) << endl;

    bSendPacket = reinterpret_cast<bool*>(bool_address);

    Util::ProtectAddr(bSendPacket, PROT_READ | PROT_WRITE | PROT_EXEC);
    //Log << "  bool value = " << *bSendPacket << endl;
}

/*
bool Hooker::HookRecvProp(const char* className, const char* propertyName, std::unique_ptr<RecvPropHook>& recvPropHook)
{
    // FIXME: Does not search recursively.. yet.
    for (ClientClass* pClass = client->GetAllClasses(); pClass; pClass = pClass->m_pNext)
    {
        if (strcmp(pClass->m_pNetworkName, className) == 0)
        {
            RecvTable* pClassTable = pClass->m_pRecvTable;

            for (int nIndex = 0; nIndex < pClassTable->m_nProps; nIndex++)
            {
                RecvProp* pProp = &pClassTable->m_pProps[nIndex];

                if (!pProp || strcmp(pProp->m_pVarName, propertyName) != 0)
                    continue;

                recvPropHook = std::make_unique<RecvPropHook>(pProp);

                return true;
            }

            break;
        }
    }

    return false;
}


void Hooker::FindGlobalVars()
{
    uintptr_t HudUpdate = reinterpret_cast<uintptr_t>(getvtable(client)[11]);

    globalVars = *reinterpret_cast<CGlobalVars**>(GetAbsoluteAddress(HudUpdate + 13, 3, 7));
}

void Hooker::FindCInput()
{
    uintptr_t IN_ActivateMouse = reinterpret_cast<uintptr_t>(getvtable(client)[15]);

    input = **reinterpret_cast<CInput***>(GetAbsoluteAddress(IN_ActivateMouse, 3, 7));
}

void Hooker::FindCGameServer()
{
    uintptr_t instruction_addr = PatternFinder::FindPatternInModule("engine_client.so", (unsigned char*) CGAMESERVER_SIGNATURE, CGAMESERVER_MASK);

    sv = *reinterpret_cast<CGameServer**>(GetAbsoluteAddress(instruction_addr, 3, 7));
}

void Hooker::FindGlowManager()
{
    uintptr_t instruction_addr = PatternFinder::FindPatternInModule("client_client.so", (unsigned char*) GLOWOBJECT_SIGNATURE, GLOWOBJECT_MASK);

    glowManager = reinterpret_cast<GlowObjectManagerFn>(GetAbsoluteAddress(instruction_addr, 1, 5))();
}

void Hooker::FindPlayerResource()
{
    uintptr_t instruction_addr = PatternFinder::FindPatternInModule("client_client.so", (unsigned char*) PLAYERRESOURCES_SIGNATURE, PLAYERRESOURCES_MASK);

    csPlayerResource = reinterpret_cast<C_CSPlayerResource**>(GetAbsoluteAddress(instruction_addr, 3, 7));
}

void Hooker::FindGameRules()
{
    uintptr_t instruction_addr = PatternFinder::FindPatternInModule("client_client.so", (unsigned char*) GAMERULES_SIGNATURE, GAMERULES_MASK);

    csGameRules = *reinterpret_cast<C_CSGameRules***>(GetAbsoluteAddress(instruction_addr, 3, 7));
}

void Hooker::FindRankReveal()
{
    uintptr_t func_addr = PatternFinder::FindPatternInModule("client_client.so", (unsigned char*) MSGFUNC_SERVERRANKREVEALALL_SIGNATURE, MSGFUNC_SERVERRANKREVEALALL_MASK);

    MsgFunc_ServerRankRevealAll = reinterpret_cast<MsgFunc_ServerRankRevealAllFn>(func_addr);
}

void Hooker::FindSendClanTag()
{
    uintptr_t func_addr = PatternFinder::FindPatternInModule("engine_client.so", (unsigned char*) SENDCLANTAG_SIGNATURE, SENDCLANTAG_MASK);

    SendClanTag = reinterpret_cast<SendClanTagFn>(func_addr);
}

void Hooker::FindViewRender()
{
    uintptr_t func_addr = PatternFinder::FindPatternInModule("client_client.so", (unsigned char*) VIEWRENDER_SIGNATURE, VIEWRENDER_MASK);

    viewRender = reinterpret_cast<CViewRender*>(GetAbsoluteAddress(func_addr + 14, 3, 7));
}

void Hooker::FindPrediction()
{
    uintptr_t seed_instruction_addr = PatternFinder::FindPatternInModule("client_client.so", (unsigned char*) PREDICTION_RANDOM_SEED_SIGNATURE, PREDICTION_RANDOM_SEED_MASK);
    uintptr_t helper_instruction_addr = PatternFinder::FindPatternInModule("client_client.so", (unsigned char*) CLIENT_MOVEHELPER_SIGNATURE, CLIENT_MOVEHELPER_MASK);
    uintptr_t movedata_instruction_addr = PatternFinder::FindPatternInModule("client_client.so", (unsigned char*) CLIENT_MOVEDATA_SIGNATURE, CLIENT_MOVEDATA_MASK);

    nPredictionRandomSeed = *reinterpret_cast<int**>(GetAbsoluteAddress(seed_instruction_addr, 3, 7));
    moveHelper = *reinterpret_cast<IMoveHelper**>(GetAbsoluteAddress(helper_instruction_addr + 1, 3, 7));
    g_MoveData = **reinterpret_cast<CMoveData***>(GetAbsoluteAddress(movedata_instruction_addr, 3, 7));
}

void Hooker::FindIsReadyCallback()
{
    uintptr_t func_addr = PatternFinder::FindPatternInModule("client_client.so", (unsigned char*) ISREADY_CALLBACK_SIGNATURE, ISREADY_CALLBACK_MASK);

    IsReadyCallback = reinterpret_cast<IsReadyCallbackFn>(func_addr);
}

void Hooker::FindSurfaceDrawing()
{
    uintptr_t start_func_addr = PatternFinder::FindPatternInModule("vguimatsurface_client.so", (unsigned char*) CMATSYSTEMSURFACE_STARTDRAWING_SIGNATURE, CMATSYSTEMSURFACE_STARTDRAWING_MASK);
    StartDrawing = reinterpret_cast<StartDrawingFn>(start_func_addr);

    uintptr_t finish_func_addr = PatternFinder::FindPatternInModule("vguimatsurface_client.so", (unsigned char*) CMATSYSTEMSURFACE_FINISHDRAWING_SIGNATURE, CMATSYSTEMSURFACE_FINISHDRAWING_MASK);
    FinishDrawing = reinterpret_cast<FinishDrawingFn>(finish_func_addr);
}

void Hooker::FindGetLocalClient()
{
    uintptr_t GetLocalPlayer = reinterpret_cast<uintptr_t>(getvtable(engine)[12]);
    GetLocalClient = reinterpret_cast<GetLocalClientFn>(GetAbsoluteAddress(GetLocalPlayer + 9, 1, 5));
}

void Hooker::FindLineGoesThroughSmoke()
{
    uintptr_t func_addr = PatternFinder::FindPatternInModule("client_client.so", (unsigned char*) LINEGOESTHROUGHSMOKE_SIGNATURE, LINEGOESTHROUGHSMOKE_MASK);
    LineGoesThroughSmoke = reinterpret_cast<LineGoesThroughSmokeFn>(func_addr);
}

void Hooker::FindGetCSWpnData()
{
    uintptr_t func_addr = PatternFinder::FindPatternInModule("client_client.so", (unsigned char*) GETCSWPNDATA_SIGNATURE, GETCSWPNDATA_MASK);
    GetCSWpnData_address = reinterpret_cast<uintptr_t*>(func_addr);
}

void Hooker::FindCrosshairWeaponTypeCheck()
{
    uintptr_t byte_address = PatternFinder::FindPatternInModule("client_client.so", (unsigned char*) CROSSHAIRWEAPONTYPECHECK_SIGNATURE, CROSSHAIRWEAPONTYPECHECK_MASK);

    CrosshairWeaponTypeCheck = reinterpret_cast<uint8_t*>(byte_address + 2);
    Util::ProtectAddr(CrosshairWeaponTypeCheck, PROT_READ | PROT_WRITE | PROT_EXEC);
}

void Hooker::FindCamThinkSvCheatsCheck()
{
    uintptr_t byte_address = PatternFinder::FindPatternInModule("client_client.so", (unsigned char*) CAMTHINK_SVCHEATSCHECK_SIGNATURE, CAMTHINK_SVCHEATSCHECK_MASK);

    CamThinkSvCheatsCheck = reinterpret_cast<uint8_t*>(byte_address);

    for (ptrdiff_t off = 0; off < 0x2; off++)
        Util::ProtectAddr(CamThinkSvCheatsCheck + off, PROT_READ | PROT_WRITE | PROT_EXEC);
}

void Hooker::HookSwapWindow()
{
    uintptr_t swapwindowFn = reinterpret_cast<uintptr_t>(dlsym(RTLD_NEXT, "SDL_GL_SwapWindow"));
    swapWindowJumpAddress = reinterpret_cast<uintptr_t*>(GetAbsoluteAddress(swapwindowFn, 3, 7));
    oSwapWindow = *swapWindowJumpAddress;
    *swapWindowJumpAddress = reinterpret_cast<uintptr_t>(&SDL2::SwapWindow);
}

void Hooker::HookPollEvent()
{
    uintptr_t polleventFn = reinterpret_cast<uintptr_t>(dlsym(RTLD_NEXT, "SDL_PollEvent"));
    polleventJumpAddress = reinterpret_cast<uintptr_t*>(GetAbsoluteAddress(polleventFn, 3, 7));
    oPollEvent = *polleventJumpAddress;
    *polleventJumpAddress = reinterpret_cast<uintptr_t>(&SDL2::PollEvent);
}

void Hooker::FindSDLInput()
{
    uintptr_t func_addr = PatternFinder::FindPatternInModule("launcher_client.so", (unsigned char*) GETSDLMGR_SIGNATURE, GETSDLMGR_MASK);

    launcherMgr = reinterpret_cast<ILauncherMgrCreateFn>(func_addr)();
}
*/
