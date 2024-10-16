#pragma once

#include <array>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#include "Engine/Data/FrameEnums.h"
#include "Engine/Time/Duration.h"

#include "Utility/Memory/Blob.h"

class GraphicsImage;

class Icon {
 public:
    inline Icon() : img(nullptr) {}

    inline void SetAnimationName(std::string_view name) {
        anim_name = name;
    }
    inline const std::string &GetAnimationName() const { return anim_name; }

    inline void SetAnimLength(Duration anim_length) {
        this->anim_length = anim_length;
    }
    inline Duration GetAnimLength() const { return this->anim_length; }

    inline void SetAnimTime(Duration anim_time) {
        this->anim_time = anim_time;
    }
    inline Duration GetAnimTime() const { return this->anim_time; }

    GraphicsImage *GetTexture();

    std::string pTextureName;
    FrameFlags uFlags;
    int id = 0;

 protected:
    std::string anim_name;
    Duration anim_length;
    Duration anim_time;
    GraphicsImage *img = nullptr;
};

struct IconFrameTable {
    Icon *GetIcon(unsigned int idx);
    Icon *GetIcon(const char *pIconName);
    unsigned int FindIcon(std::string_view pIconName);
    Icon *GetFrame(unsigned int uIconID, Duration frame_time);

    std::vector<Icon> pIcons;
};

extern IconFrameTable *pIconsFrameTable;
