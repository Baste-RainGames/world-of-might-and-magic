#pragma once

#include "GUI/UI/UIHouses.h"
#include "GUI/UI/UIHouseEnums.h"

void MercenaryGuildDialog();

class GUIWindow_NormalHouse : public GUIWindow_House {
 public:
    explicit GUIWindow_NormalHouse(HouseId houseId) : GUIWindow_House(houseId) {}
    virtual ~GUIWindow_NormalHouse() {}

    /**
     * @offset 0x4B6478
     *
     * @brief Mercenary guild in MM6.
     */
    virtual void houseDialogueOptionSelected(DialogueId option) override;
    virtual void houseSpecificDialogue() override;
};
