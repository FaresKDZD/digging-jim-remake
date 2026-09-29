#include "Cave/Properties/Properties.h"
#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/slider.h>
#include <wx/checkbox.h>
#include <wx/combo.h>
#include <wx/checklst.h>
#include <algorithm>

namespace {

struct ColorClipboard {
    bool valid = false;
    uint32_t hue = 100;
    uint32_t sat = 100;
    uint32_t lum = 100;
};

ColorClipboard g_colorClipboard;

void ensureWxInit()
{
    if (wxTheApp) return;
    static wxApp s_app;
    wxApp::SetInstance(&s_app);
    int argc = 0;
    wxEntryStart(argc, static_cast<wchar_t**>(nullptr));
}

class PropertiesDialog : public wxDialog
{
public:
    PropertiesDialog(Cave::Properties& props, int diamondCount, bool /*developerMode*/, std::function<void()> onTest, bool* editableBorders)
        : wxDialog(nullptr, wxID_ANY, "Cave Properties",
                   wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
        , m_props(props)
        , m_diamondCount(diamondCount)
        , m_onTest(std::move(onTest))
        , m_editableBordersPtr(editableBorders)
    {
        auto* outer = new wxBoxSizer(wxVERTICAL);

        // ── Size: Width / Height ──────────────────────────────────────────────
        {
            auto* box  = new wxStaticBoxSizer(wxVERTICAL, this, "Size");
            wxWindow* p = box->GetStaticBox();

            auto* grid = new wxFlexGridSizer(2, 2, 6, 10);
            grid->Add(lbl(p, "Width:"),  0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
            m_width  = spin(p, 20, 255, props.width);
            grid->Add(m_width,  0, wxALIGN_CENTER_VERTICAL);
            grid->Add(lbl(p, "Height:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
            m_height = spin(p, 13, 255, props.height);
            grid->Add(m_height, 0, wxALIGN_CENTER_VERTICAL);

            auto* row = new wxBoxSizer(wxHORIZONTAL);
            row->Add(grid, 0, wxALIGN_CENTER_VERTICAL);
            if (m_editableBordersPtr) {
                m_editableBorders = new wxCheckBox(p, wxID_ANY, "Editable borders");
                m_editableBorders->SetValue(*m_editableBordersPtr);
                row->Add(m_editableBorders, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 24);
            }
            box->Add(row, 0, wxALL, 10);
            outer->Add(box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
        }

        // ── Top row: Diamonds (left) | Amoeba+Time (right) ───────────────────
        auto* topRow = new wxBoxSizer(wxHORIZONTAL);

        {
            auto* box  = new wxStaticBoxSizer(wxVERTICAL, this, "");
            wxWindow* p = box->GetStaticBox();
            auto* grid = new wxFlexGridSizer(3, 3, 6, 10);

            grid->Add(lbl(p, "Diamonds to collect:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
            m_quota = spin(p, QUOTA_MIN, QUOTA_MAX, props.quota);
            grid->Add(m_quota, 0, wxALIGN_CENTER_VERTICAL);
            m_btnCount = new wxButton(p, wxID_ANY, "Count", wxDefaultPosition, wxSize(120, -1));
            grid->Add(m_btnCount, 0, wxALIGN_CENTER_VERTICAL);

            grid->Add(lbl(p, "Diamond value:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
            m_diamVal = spin(p, DIAMOND_VALUE_MIN, DIAMOND_VALUE_MAX, props.diamondValue);
            grid->Add(m_diamVal, 0, wxALIGN_CENTER_VERTICAL);
            grid->AddSpacer(0);

            grid->Add(lbl(p, "Extra diamond value:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
            m_extraDiamVal = spin(p, EXTRA_DIAMOND_VALUE_MIN, EXTRA_DIAMOND_VALUE_MAX, props.extraDiamondValue);
            grid->Add(m_extraDiamVal, 0, wxALIGN_CENTER_VERTICAL);
            grid->AddSpacer(0);

            box->Add(grid, 0, wxALL, 10);
            topRow->Add(box, 1, wxEXPAND | wxRIGHT, 8);
        }

        {
            auto* rightCol = new wxBoxSizer(wxVERTICAL);

            {
                auto* box  = new wxStaticBoxSizer(wxVERTICAL, this, "");
                wxWindow* p = box->GetStaticBox();
                auto* grid = new wxFlexGridSizer(1, 2, 6, 10);
                grid->Add(lbl(p, "Amoeba growth max:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
                m_amoebaMax = spin(p, AMOEBA_GRWOTH_MAX_MIN, AMOEBA_GRWOTH_MAX_MAX, props.amoebaGrowthMax);
                grid->Add(m_amoebaMax, 0, wxALIGN_CENTER_VERTICAL);
                box->Add(grid, 0, wxALL, 10);
                rightCol->Add(box, 0, wxEXPAND | wxBOTTOM, 8);
            }

            {
                auto* box  = new wxStaticBoxSizer(wxVERTICAL, this, "");
                wxWindow* p = box->GetStaticBox();
                auto* grid = new wxFlexGridSizer(3, 2, 6, 10);
                grid->Add(lbl(p, "Time:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
                m_time = spin(p, TIME_MIN, TIME_MAX, props.unlimitedTime ? TIME_MAX : props.time);
                grid->Add(m_time, 0, wxALIGN_CENTER_VERTICAL);
                grid->AddSpacer(0);
                m_unlimitedTime = new wxCheckBox(p, wxID_ANY, "Unlimited time");
                m_unlimitedTime->SetValue(props.unlimitedTime);
                grid->Add(m_unlimitedTime, 0, wxALIGN_CENTER_VERTICAL);
                grid->Add(lbl(p, "Magic wall expire time:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
                m_magicTime = spin(p, MAGIC_WALL_TIME_MIN, MAGIC_WALL_TIME_MAX, props.magicWallTime);
                grid->Add(m_magicTime, 0, wxALIGN_CENTER_VERTICAL);
                box->Add(grid, 0, wxALL, 10);
                rightCol->Add(box, 0, wxEXPAND);

                m_time->Enable(!props.unlimitedTime);
                m_unlimitedTime->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
                    const bool on = m_unlimitedTime->GetValue();
                    m_time->Enable(!on);
                    if (on) m_time->SetValue(static_cast<int>(TIME_MAX));
                });
            }

            topRow->Add(rightCol, 1, wxEXPAND);
        }

        outer->Add(topRow, 0, wxEXPAND | wxALL, 8);

        // ── Amoeba / Plasma / Chum growth ─────────────────────────────────────
        outer->Add(sliderGroup("Amoeba growth speed",
                               m_amoebaSpd, props.amoebaGrowthSpeed,
                               AMOEBA_GROWTH_SPEED_MIN, AMOEBA_GROWTH_SPEED_MAX, 6),
                   0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

        outer->Add(sliderGroup("Plasma growth speed",
                               m_plasmaSpd, props.plasmaGrowthSpeed,
                               PLASMA_GROWTH_SPEED_MIN, PLASMA_GROWTH_SPEED_MAX, 6),
                   0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

        outer->Add(sliderGroup("Chum growth speed",
                               m_chumSpd, props.chumGrowthSpeed,
                               CHUM_GROWTH_SPEED_MIN, CHUM_GROWTH_SPEED_MAX, 6),
                   0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

        outer->Add(sliderGroup("Lava growth speed",
                               m_lavaSpd, props.lavaGrowthSpeed,
                               LAVA_GROWTH_SPEED_MIN, LAVA_GROWTH_SPEED_MAX, 6),
                   0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

        // ── Colors ────────────────────────────────────────────────────────────
        {
            auto* box  = new wxStaticBoxSizer(wxVERTICAL, this, "Colors");
            wxWindow* p = box->GetStaticBox();
            auto* grid = new wxFlexGridSizer(3, 2, 6, 10);
            grid->AddGrowableCol(1);

            grid->Add(lbl(p, "Hue"),   0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
            m_hue = new wxSlider(p, wxID_ANY, props.hue,   HUE_MIN, HUE_MAX);
            grid->Add(m_hue, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

            grid->Add(lbl(p, "Sat"),   0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
            m_sat = new wxSlider(p, wxID_ANY, props.sat,   SAT_MIN, SAT_MAX);
            grid->Add(m_sat, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

            grid->Add(lbl(p, "Light"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
            m_lum = new wxSlider(p, wxID_ANY, props.lum,   LUM_MIN, LUM_MAX);
            grid->Add(m_lum, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

            box->Add(grid, 0, wxEXPAND | wxALL, 10);

            auto* copyRow = new wxBoxSizer(wxHORIZONTAL);
            m_btnCopy  = new wxButton(p, wxID_ANY, "Copy");
            m_btnPaste = new wxButton(p, wxID_ANY, "Paste");
            m_btnPaste->Enable(g_colorClipboard.valid);
            copyRow->Add(m_btnCopy,  0, wxRIGHT, 8);
            copyRow->Add(m_btnPaste);
            box->Add(copyRow, 0, wxLEFT | wxBOTTOM, 10);

            auto* btnRow = new wxBoxSizer(wxHORIZONTAL);
            m_btnTest  = new wxButton(p, wxID_ANY, "Test");
            m_btnReset = new wxButton(p, wxID_ANY, "Reset");
            btnRow->Add(m_btnTest,  0, wxRIGHT, 8);
            btnRow->Add(m_btnReset);
            box->Add(btnRow, 0, wxLEFT | wxBOTTOM, 10);

            outer->Add(box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
        }

        outer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALL | wxALIGN_RIGHT, 8);

        SetSizerAndFit(outer);
        Centre();

        m_btnCount->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            m_quota->SetValue(m_diamondCount);
        });
        m_btnTest->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            ApplyToProps();
            if (m_onTest) m_onTest();
        });
        m_btnReset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            m_hue->SetValue(100);
            m_sat->SetValue(100);
            m_lum->SetValue(100);
        });
        m_btnCopy->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            g_colorClipboard.hue = (uint32_t)std::clamp(m_hue->GetValue(), (int)HUE_MIN, (int)HUE_MAX);
            g_colorClipboard.sat = (uint32_t)std::clamp(m_sat->GetValue(), (int)SAT_MIN, (int)SAT_MAX);
            g_colorClipboard.lum = (uint32_t)std::clamp(m_lum->GetValue(), (int)LUM_MIN, (int)LUM_MAX);
            g_colorClipboard.valid = true;
            m_btnPaste->Enable(true);
        });
        m_btnPaste->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            if (!g_colorClipboard.valid) return;
            m_hue->SetValue((int)std::clamp(g_colorClipboard.hue, HUE_MIN, HUE_MAX));
            m_sat->SetValue((int)std::clamp(g_colorClipboard.sat, SAT_MIN, SAT_MAX));
            m_lum->SetValue((int)std::clamp(g_colorClipboard.lum, LUM_MIN, LUM_MAX));
        });
    }

    void ApplyToProps()
    {
        auto clampGet = [](wxSpinCtrl* s, uint32_t lo, uint32_t hi) -> uint32_t {
            return (uint32_t)std::clamp(s->GetValue(), (int)lo, (int)hi);
        };
        auto clampSlider = [](wxSlider* s, uint32_t lo, uint32_t hi) -> uint32_t {
            return (uint32_t)std::clamp(s->GetValue(), (int)lo, (int)hi);
        };

        if (m_width && m_height) {
            m_props.width  = (uint32_t)std::clamp(m_width->GetValue(),  20, 255);
            m_props.height = (uint32_t)std::clamp(m_height->GetValue(), 13, 255);
        }
        m_props.quota             = clampGet(m_quota,        QUOTA_MIN,               QUOTA_MAX);
        m_props.diamondValue      = clampGet(m_diamVal,      DIAMOND_VALUE_MIN,        DIAMOND_VALUE_MAX);
        m_props.extraDiamondValue = clampGet(m_extraDiamVal, EXTRA_DIAMOND_VALUE_MIN,  EXTRA_DIAMOND_VALUE_MAX);
        m_props.amoebaGrowthMax   = clampGet(m_amoebaMax,    AMOEBA_GRWOTH_MAX_MIN,    AMOEBA_GRWOTH_MAX_MAX);
        m_props.unlimitedTime     = m_unlimitedTime && m_unlimitedTime->GetValue();
        m_props.time              = m_props.unlimitedTime
            ? TIME_MAX
            : clampGet(m_time, TIME_MIN, TIME_MAX);
        m_props.magicWallTime     = clampGet(m_magicTime,    MAGIC_WALL_TIME_MIN,      MAGIC_WALL_TIME_MAX);
        m_props.amoebaGrowthSpeed = clampSlider(m_amoebaSpd, AMOEBA_GROWTH_SPEED_MIN,  AMOEBA_GROWTH_SPEED_MAX);
        m_props.plasmaGrowthSpeed = clampSlider(m_plasmaSpd, PLASMA_GROWTH_SPEED_MIN,  PLASMA_GROWTH_SPEED_MAX);
        m_props.chumGrowthSpeed   = clampSlider(m_chumSpd,   CHUM_GROWTH_SPEED_MIN,    CHUM_GROWTH_SPEED_MAX);
        m_props.lavaGrowthSpeed   = clampSlider(m_lavaSpd,   LAVA_GROWTH_SPEED_MIN,    LAVA_GROWTH_SPEED_MAX);
        m_props.hue               = clampSlider(m_hue,       HUE_MIN,                  HUE_MAX);
        m_props.sat               = clampSlider(m_sat,       SAT_MIN,                  SAT_MAX);
        m_props.lum               = clampSlider(m_lum,       LUM_MIN,                  LUM_MAX);
        if (m_editableBorders && m_editableBordersPtr)
            *m_editableBordersPtr = m_editableBorders->GetValue();
    }

private:
    wxStaticText* lbl(wxWindow* parent, const char* text)
    {
        return new wxStaticText(parent, wxID_ANY, text);
    }

    wxSpinCtrl* spin(wxWindow* parent, uint32_t lo, uint32_t hi, uint32_t val)
    {
        auto* s = new wxSpinCtrl(parent, wxID_ANY, wxEmptyString,
                                 wxDefaultPosition, wxDefaultSize);
        s->SetRange((int)lo, (int)hi);
        s->SetValue((int)val);
        return s;
    }

    wxStaticBoxSizer* sliderGroup(const char* title, wxSlider*& outSlider,
                                  uint32_t val, uint32_t lo, uint32_t hi, int pad = 10)
    {
        auto* box = new wxStaticBoxSizer(wxVERTICAL, this, title);
        wxWindow* p = box->GetStaticBox();
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(p, wxID_ANY, "Min"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        outSlider = new wxSlider(p, wxID_ANY, (int)std::clamp(val, lo, hi), (int)lo, (int)hi);
        row->Add(outSlider, 1, wxEXPAND);
        row->Add(new wxStaticText(p, wxID_ANY, "Max"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
        box->Add(row, 0, wxEXPAND | wxALL, pad);
        return box;
    }

    Cave::Properties&       m_props;
    int                     m_diamondCount;
    std::function<void()>   m_onTest;
    bool*                   m_editableBordersPtr = nullptr;

    wxSpinCtrl*    m_width           = nullptr;
    wxSpinCtrl*    m_height          = nullptr;
    wxCheckBox*    m_editableBorders = nullptr;

    wxSpinCtrl* m_quota        = nullptr;
    wxSpinCtrl* m_diamVal      = nullptr;
    wxSpinCtrl* m_extraDiamVal = nullptr;
    wxSpinCtrl* m_amoebaMax    = nullptr;
    wxSpinCtrl* m_time         = nullptr;
    wxCheckBox* m_unlimitedTime = nullptr;
    wxSpinCtrl* m_magicTime    = nullptr;
    wxSlider*   m_amoebaSpd    = nullptr;
    wxSlider*   m_plasmaSpd    = nullptr;
    wxSlider*   m_chumSpd      = nullptr;
    wxSlider*   m_lavaSpd      = nullptr;
    wxSlider*   m_hue          = nullptr;
    wxSlider*   m_sat          = nullptr;
    wxSlider*   m_lum          = nullptr;
    wxButton*   m_btnCount     = nullptr;
    wxButton*   m_btnCopy      = nullptr;
    wxButton*   m_btnPaste     = nullptr;
    wxButton*   m_btnTest      = nullptr;
    wxButton*   m_btnReset     = nullptr;
};

} // namespace

void Cave::editCaveProperties(sf::RenderWindow&, Cave::Properties& props,
                               Cave::Properties original, const int& diamondCount,
                               bool& trigger, bool developerMode,
                               std::function<void()> onTest, bool* editableBorders)
{
    ensureWxInit();
    const bool bordersBackup = editableBorders ? *editableBorders : false;
    PropertiesDialog dlg(props, diamondCount, developerMode, std::move(onTest), editableBorders);
    if (dlg.ShowModal() == wxID_OK)
        dlg.ApplyToProps();
    else {
        props = original;
        if (editableBorders)
            *editableBorders = bordersBackup;
    }
    trigger = true;
}

namespace {

class VitusMonsterPopup : public wxComboPopup
{
public:
    bool Create(wxWindow* parent) override
    {
        m_list = new wxCheckListBox(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 220));
        using C = Cave::Entity::Cosmic;
        for (int i = 0; i < C::VITUS_OPTION_COUNT; ++i)
            m_list->Append(C::VITUS_OPTIONS[i].label);
        ApplyChecks();
        m_list->Bind(wxEVT_CHECKLISTBOX, [this](wxCommandEvent&) {
            if (GetComboCtrl())
                GetComboCtrl()->SetText(GetStringValue());
        });
        return true;
    }

    wxWindow* GetControl() override { return m_list; }

    wxString GetStringValue() const override
    {
        if (!m_list) return wxString();
        int n = 0;
        const unsigned count = m_list->GetCount();
        for (unsigned i = 0; i < count; ++i)
            if (m_list->IsChecked(i)) ++n;
        if (n == 0) return "None";
        if (n == static_cast<int>(count)) return "All";
        return wxString::Format("%d selected", n);
    }

    wxSize GetAdjustedSize(int minWidth, int, int maxHeight) override
    {
        const int h = maxHeight > 0 ? std::min(maxHeight, 240) : 240;
        return wxSize(std::max(minWidth, 180), h);
    }

    void SetChecks(uint32_t mask)
    {
        m_mask = mask;
        ApplyChecks();
    }

    uint32_t GetMask() const
    {
        if (!m_list) return m_mask;
        uint32_t mask = 0;
        for (unsigned i = 0; i < m_list->GetCount(); ++i)
            if (m_list->IsChecked(i))
                mask |= (1u << i);
        return mask;
    }

private:
    void ApplyChecks()
    {
        if (!m_list) return;
        for (unsigned i = 0; i < m_list->GetCount(); ++i)
            m_list->Check(i, (m_mask & (1u << i)) != 0);
    }

    wxCheckListBox* m_list = nullptr;
    uint32_t m_mask = Cave::Entity::Cosmic::defaultVitusMask();
};

class CosmicSettingsDialog : public wxDialog
{
public:
    explicit CosmicSettingsDialog(Cave::Entity::Cosmic::Settings& settings)
        : wxDialog(nullptr, wxID_ANY, "Cosmic Settings",
                   wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
        , m_settings(settings)
    {
        auto* outer = new wxBoxSizer(wxVERTICAL);

        {
            auto* box = new wxStaticBoxSizer(wxVERTICAL, this, "");
            wxWindow* p = box->GetStaticBox();
            auto* grid = new wxFlexGridSizer(5, 2, 6, 10);

            grid->Add(lbl(p, "Murus max boulders:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
            m_boulders = spin(p, 0, 255, settings.maxBoulders);
            grid->Add(m_boulders, 0, wxALIGN_CENTER_VERTICAL);

            grid->Add(lbl(p, "Murus max brick walls:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
            m_walls = spin(p, 0, 255, settings.maxWalls);
            grid->Add(m_walls, 0, wxALIGN_CENTER_VERTICAL);

            grid->Add(lbl(p, "Adama max diamonds:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
            m_diamonds = spin(p, 0, 255, settings.maxDiamonds);
            grid->Add(m_diamonds, 0, wxALIGN_CENTER_VERTICAL);

            grid->Add(lbl(p, "Vitus max monsters:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
            m_monsters = spin(p, 0, 255, settings.maxMonsters);
            grid->Add(m_monsters, 0, wxALIGN_CENTER_VERTICAL);

            grid->Add(lbl(p, "Vitus spawn list:"), 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
            m_vitusCombo = new wxComboCtrl(p, wxID_ANY, wxEmptyString,
                wxDefaultPosition, wxSize(180, -1), wxCB_READONLY);
            m_vitusPopup = new VitusMonsterPopup();
            m_vitusPopup->SetChecks(settings.monsterMask);
            m_vitusCombo->SetPopupControl(m_vitusPopup);
            m_vitusCombo->SetText(m_vitusPopup->GetStringValue());
            grid->Add(m_vitusCombo, 0, wxALIGN_CENTER_VERTICAL);

            box->Add(grid, 0, wxALL, 10);
            outer->Add(box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
        }

        outer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxALL | wxALIGN_RIGHT, 8);
        SetSizerAndFit(outer);
        Centre();
    }

    void ApplyToSettings()
    {
        auto clampGet = [](wxSpinCtrl* s) -> uint8_t {
            return static_cast<uint8_t>(std::clamp(s->GetValue(), 0, 255));
        };
        m_settings.maxBoulders = clampGet(m_boulders);
        m_settings.maxWalls = clampGet(m_walls);
        m_settings.maxDiamonds = clampGet(m_diamonds);
        m_settings.maxMonsters = clampGet(m_monsters);
        m_settings.monsterMask = m_vitusPopup ? m_vitusPopup->GetMask() : Cave::Entity::Cosmic::defaultVitusMask();
    }

private:
    wxStaticText* lbl(wxWindow* parent, const char* text)
    {
        return new wxStaticText(parent, wxID_ANY, text);
    }

    wxSpinCtrl* spin(wxWindow* parent, uint32_t lo, uint32_t hi, uint32_t val)
    {
        auto* s = new wxSpinCtrl(parent, wxID_ANY, wxEmptyString,
                                 wxDefaultPosition, wxDefaultSize);
        s->SetRange((int)lo, (int)hi);
        s->SetValue((int)val);
        return s;
    }

    Cave::Entity::Cosmic::Settings& m_settings;
    wxSpinCtrl* m_boulders = nullptr;
    wxSpinCtrl* m_walls = nullptr;
    wxSpinCtrl* m_diamonds = nullptr;
    wxSpinCtrl* m_monsters = nullptr;
    wxComboCtrl* m_vitusCombo = nullptr;
    VitusMonsterPopup* m_vitusPopup = nullptr;
};

} // namespace

void Cave::editCosmicSettings(sf::RenderWindow&, Cave::Entity::Cosmic::Settings& settings,
    Cave::Entity::Cosmic::Settings original, bool& trigger)
{
    ensureWxInit();
    CosmicSettingsDialog dlg(settings);
    if (dlg.ShowModal() == wxID_OK)
        dlg.ApplyToSettings();
    else
        settings = original;
    trigger = true;
}
