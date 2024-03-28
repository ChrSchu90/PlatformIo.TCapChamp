#include <ESPUI.h>
#include "TemperatureTab.h"
/*
##############################################
##           TemperatureAreaTab             ##
##############################################
*/

/// @brief Creates a new tab for the temperature area configuration inside the Web interface
/// @param tab the parent temperature tab
/// @param config the temperature area configuration
TemperatureAreaTab::TemperatureAreaTab(TemperatureTab *tab, TemperatureArea *config) : _tab(tab), _config(config)
{
    // ESPUI.addControl(Separator, "Areas", "", None, tab->getTabId());

    _swEnabled = ESPUI.addControl(
        Switcher, config->name.c_str(), String(config->isEnabled() ? 1 : 0), None, tab->getTabId(),
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            bool newValue = sender->value.toInt() > 0;
            instance->_config->setEnabled(newValue);
            ESPUI.updateSwitcher(sender->id, newValue);
            instance->updateStatus();
        },
        this);

    // Empty label for spaceing
    ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, _swEnabled), "background-color: unset; width: 100%;");

    ESPUI.setElementStyle(ESPUI.addControl(Label, "", "Start °C", None, _swEnabled), "background-color: unset; width: 100%;");
    _sldStart = ESPUI.addControl(
        Slider, "Start", String(config->getStart()), None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            int newValue = sender->value.toInt();
            instance->_config->setStart(newValue);
            ESPUI.updateSlider(sender->id, newValue);
            instance->updateStatus();
        },
        this);
    ESPUI.addControl(Min, "", "-20", None, _sldStart);
    ESPUI.addControl(Max, "", "30", None, _sldStart);
    ESPUI.addControl(Step, "", "1", None, _sldStart);

    ESPUI.setElementStyle(ESPUI.addControl(Label, "", "End °C", None, _swEnabled), "background-color: unset; width: 100%;");
    _sldEnd = ESPUI.addControl(
        Slider, "End", String(config->getEnd()), None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            int newValue = sender->value.toInt();
            instance->_config->setEnd(newValue);
            ESPUI.updateSlider(sender->id, newValue);
            instance->updateStatus();
        },
        this);
    ESPUI.addControl(Min, "", "-20", None, _sldEnd);
    ESPUI.addControl(Max, "", "30", None, _sldEnd);
    ESPUI.addControl(Step, "", "1", None, _sldEnd);

    ESPUI.setElementStyle(ESPUI.addControl(Label, "", "Offset °C", None, _swEnabled), "background-color: unset; width: 100%;");
    _sldOffset = ESPUI.addControl(
        Slider, "Offset", String(config->getOffset()), None, _swEnabled,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureAreaTab *instance = static_cast<TemperatureAreaTab *>(UserInfo);
            int newValue = sender->value.toInt();
            instance->_config->setOffset(newValue);
            ESPUI.updateSlider(sender->id, newValue);
        },
        this);
    ESPUI.addControl(Min, "", "-20", None, _sldOffset);
    ESPUI.addControl(Max, "", "20", None, _sldOffset);
    ESPUI.addControl(Step, "", "1", None, _sldOffset);

    updateStatus();
}

/// @brief Forces an update of the UI values
void TemperatureAreaTab::update()
{
    ESPUI.updateSwitcher(_swEnabled, _config->isEnabled());
    ESPUI.updateSlider(_sldStart, _config->getStart());
    ESPUI.updateSlider(_sldEnd, _config->getEnd());
    ESPUI.updateSlider(_sldOffset, _config->getOffset());

    updateStatus();
}

/// @brief Foreces an update of the status indicator to make a invalid configuration visible
void TemperatureAreaTab::updateStatus()
{
    ESPUI.setEnabled(_sldStart, _config->isEnabled());
    ESPUI.setEnabled(_sldEnd, _config->isEnabled());
    ESPUI.setEnabled(_sldOffset, _config->isEnabled());

    if (_config->isEnabled() && !_config->isValid())
    {
        ESPUI.setPanelStyle(_swEnabled, "background-color: #E74C3C;"); // red
    }
    else
    {
        ESPUI.setPanelStyle(_swEnabled, "background-color: #444857;"); // none
    }
}

/*
##############################################
##             TemperatureTab               ##
##############################################
*/

/// @brief Creates a new tab for the temperature configuration inside the Web interface
/// @param config the temperature configuration
TemperatureTab::TemperatureTab(TemperatureConfig *config) : _config(config)
{
    _tab = ESPUI.addControl(Tab, "Temperature", "Temperature");
    _swManualMode = ESPUI.addControl(
        Switcher, "Manual Temperature", String(config->isManualMode() ? 1 : 0), None, _tab,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureTab *instance = static_cast<TemperatureTab *>(UserInfo);
            bool newValue = sender->value.toInt() > 0;
            instance->_config->setManualMode(newValue);
            ESPUI.updateSwitcher(sender->id, newValue);
            instance->updateStatus();
        },
        this);

    _sldManualTemp = ESPUI.addControl(
        Slider, "Temperature", String(config->getManualTemperature()), None, _swManualMode,
        [](Control *sender, int type, void *UserInfo)
        {
            TemperatureTab *instance = static_cast<TemperatureTab *>(UserInfo);
            int newValue = sender->value.toInt();
            instance->_config->setManualTemperature(newValue);
            ESPUI.updateSlider(sender->id, newValue);
        },
        this);
    ESPUI.addControl(Min, "", "-20", None, _sldManualTemp);
    ESPUI.addControl(Max, "", "30", None, _sldManualTemp);
    ESPUI.addControl(Step, "", "1", None, _sldManualTemp);

    for (size_t i = 0; i < TEMP_AREA_AMOUNT; i++)
    {
        _areas[i] = new TemperatureAreaTab(this, config->getArea(i));
    }

    updateStatus();
}

/// @brief Forces an update of the UI values
void TemperatureTab::update()
{
    ESPUI.updateSwitcher(_swManualMode, _config->isManualMode());
    ESPUI.updateSlider(_sldManualTemp, _config->getManualTemperature());
    for (size_t i = 0; i < TEMP_AREA_AMOUNT; i++)
    {
        _areas[i]->update();
    }

    updateStatus();
}

/// @brief Foreces an update of the status indicator to make a invalid configuration visible
void TemperatureTab::updateStatus()
{
    ESPUI.setEnabled(_sldManualTemp, _config->isManualMode());
}