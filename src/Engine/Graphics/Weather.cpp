#include "Engine/Graphics/Renderer/Renderer.h"

#include "Engine/Engine.h"

#include "Engine/Graphics/Viewport.h"
#include "Engine/Graphics/Weather.h"
#include "Engine/Random/Random.h"

Weather *pWeather = new Weather;

void Weather::DrawSnow() {
    for (unsigned int i = 0; i < Screen_Coord.size(); ++i) {
        int size = 1;
        int base = 3;
        if (i >= 950) {
            size = 4;
            base = 10;
        } else if (i >= 700) {
            size = 2;
            base = 5;
        }

        Screen_Coord[i].x += vrng->random(base) - base / 2;
        Screen_Coord[i].y += vrng->random(size) + size;
        if (Screen_Coord[i].x < pViewport->uScreen_TL_X) {
            Screen_Coord[i].x = pViewport->uScreen_TL_X + vrng->random(base);
        } else if (Screen_Coord[i].x >= (pViewport->uScreen_BR_X - size)) {
            Screen_Coord[i].x = pViewport->uScreen_BR_X - vrng->random(base);
        }
        if (Screen_Coord[i].y >= (pViewport->uScreen_BR_Y - size)) {
            Screen_Coord[i].y = pViewport->uScreen_TL_Y;
            Screen_Coord[i].x = pViewport->uScreen_TL_X + vrng->random(pViewport->uScreen_BR_X - pViewport->uScreen_TL_X - size);
        }

        render->FillRectFast(Screen_Coord[i].x, Screen_Coord[i].y, size, size, colorTable.White);
    }
}

void Weather::Initialize() {
    int width = pViewport->uScreen_BR_X - pViewport->uScreen_TL_X;
    int height = pViewport->uScreen_BR_Y - pViewport->uScreen_TL_Y;
    for (Pointi &point : Screen_Coord) {
        point.x = pViewport->uScreen_TL_X + vrng->random(width);
        point.y = pViewport->uScreen_TL_Y + vrng->random(height);
    }
}

void Weather::Draw() {
    if (bRenderSnow && engine->config->graphics.Snow.value()) {
        DrawSnow();
    }
}

bool Weather::OnPlayerTurn(int dangle) {
    if (!bRenderSnow) {
        return false;
    }

    unsigned int screen_width = pViewport->uScreen_BR_X - pViewport->uScreen_TL_X;

    for (Pointi &point : Screen_Coord) {
        point.x += dangle;
        if (point.x < pViewport->uScreen_BR_X) {
            if (point.x >= pViewport->uScreen_TL_X) {
                continue;
            }
            point.x += screen_width;
        } else {
            point.x -= screen_width;
        }
    }

    return true;
}
