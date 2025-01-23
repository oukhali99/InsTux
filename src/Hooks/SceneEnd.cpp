#include "hooks.h"

void Hooks::SceneEnd(void* thisptr)
{
    if (engine->IsTakingScreenshot()) {
        renderViewVMT->GetOriginalMethod<SceneEndFn>(SCENE_END_IDX)(thisptr);
        return;
    }

    for (int i = 0; i < entityList->GetHighestEntityIndex(); i++) Chams::EntitySceneEndHook(thisptr, entityList->GetClientEntity(i));
    renderViewVMT->GetOriginalMethod<SceneEndFn>(SCENE_END_IDX)(thisptr);
}
