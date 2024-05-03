#include "GameFSMBuilder.h"

#include <Engine/Engine.h>
#include <Library/Fsm/FSMBuilder.h>

#include <utility>
#include <memory>

#include "VideoState.h"
#include "LoadStep2State.h"
#include "MainMenuState.h"

std::unique_ptr<FSM> GameFSMBuilder::buildFSM() {
    FSMBuilder fsmBuilder;
    _buildIntroVideoSequence(fsmBuilder);

    auto fsm = fsmBuilder.build();
    _setStartingState(*fsm);
    return std::move(fsm);
}

void GameFSMBuilder::_setStartingState(FSM &fsm) {
    GameConfig::Debug &debugConfig = engine->config->debug;
    if (!debugConfig.NoVideo.value()) {
        if (!debugConfig.NoLogo.value()) {
            fsm.jumpToState("3DOVideo");
        } else if (!debugConfig.NoIntro.value()) {
            fsm.jumpToState("IntroVideo");
        }
    }
}

void GameFSMBuilder::_buildIntroVideoSequence(FSMBuilder &builder) {
    builder
    .state<VideoState>("3DOVideo", "3dologo")
        .on("videoEnd").jumpTo("NWCVideo")

    .state<VideoState>("NWCVideo", "new world logo")
        .on("videoEnd").jumpTo("JVCVideo")

    .state<VideoState>("JVCVideo", "jvc")
        .on("videoEnd")
            .jumpTo([]() { return !engine->config->debug.NoIntro.value(); }, "IntroVideo")
            .jumpTo("_Exit")

    .state<VideoState>("IntroVideo", "Intro")
        .on("videoEnd").jumpTo("LoadStep2")

    .state<LoadStep2State>("LoadStep2")
        .on("done").jumpTo("MainMenu")

    .state<MainMenuState>("MainMenu")
        .on("newGame").jumpTo("_Exit")
        .on("loadGame").jumpTo("_Exit")
        .on("credits").jumpTo("_Exit")
        .on("exit").jumpTo("_Exit");
}
