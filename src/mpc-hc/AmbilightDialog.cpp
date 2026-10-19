#include "stdafx.h"
#include "AmbilightDialog.h"

BEGIN_MESSAGE_MAP(CAmbilightDialog, CMPCThemeDialog)
    ON_WM_HSCROLL()
    ON_WM_TIMER()
END_MESSAGE_MAP()

CAmbilightDialog::CAmbilightDialog(CWnd* parent, ConfigProvider provider)
    : CMPCThemeDialog(IDD_AMBILIGHT, parent), m_provider(std::move(provider)) {}

CAmbilightDialog::~CAmbilightDialog() {
    if (GetSafeHwnd()) DestroyWindow();
}

CComPtr<IExFilterConfig> CAmbilightDialog::GetConfig() {
    auto config = m_provider();
    bool enabled;
    if (!config || FAILED(config->Flt_GetBool("ambilightEnabled", &enabled))) return nullptr;
    return config;
}

BOOL CAmbilightDialog::OnInitDialog() {
    __super::OnInitDialog();
    m_loading = true;
    for (size_t i = 0; i < AmbilightSliderCount; ++i) {
        const auto& definition = AmbilightSettingDefinitions[i];
        m_sliders[i].SubclassDlgItem(IDC_AMBILIGHT_SLIDER + (UINT)i, this);
        m_sliders[i].SetRange(definition.minimum, definition.maximum);
        m_sliders[i].SetPageSize(std::max(1, (definition.maximum - definition.minimum) / 20));
        SetDlgItemText(IDC_AMBILIGHT_LABEL + (UINT)i, definition.label);
    }
    m_loading = false;
    fulfillThemeReqs();
    RefreshControls();
    SetTimer(1, 500, nullptr);
    return TRUE;
}

void CAmbilightDialog::RefreshControls() {
    if (!GetSafeHwnd()) return;
    auto config = GetConfig();
    m_connected = !!config;
    m_loading = true;
    bool enabled = false;
    if (config) config->Flt_GetBool("ambilightEnabled", &enabled);
    CheckDlgButton(IDC_AMBILIGHT_ENABLED, enabled ? BST_CHECKED : BST_UNCHECKED);
    GetDlgItem(IDC_AMBILIGHT_ENABLED)->EnableWindow(m_connected);
    GetDlgItem(IDC_AMBILIGHT_RESET)->EnableWindow(m_connected);
    for (size_t i = 0; i < AmbilightSliderCount; ++i) {
        const auto& definition = AmbilightSettingDefinitions[i];
        int value = definition.initial;
        if (config) config->Flt_GetInt(definition.key, &value);
        m_sliders[i].SetPos(value);
        m_sliders[i].EnableWindow(m_connected);
        GetDlgItem(IDC_AMBILIGHT_VALUE + (UINT)i)->EnableWindow(m_connected);
        SetDlgItemInt(IDC_AMBILIGHT_VALUE + (UINT)i, value, TRUE);
    }
    int directions = 15;
    if (config) config->Flt_GetInt("ambilightDirections", &directions);
    for (int i = 0; i < 4; ++i) {
        CheckDlgButton(IDC_AMBILIGHT_TOP + i, (directions & (1 << i)) ? BST_CHECKED : BST_UNCHECKED);
        GetDlgItem(IDC_AMBILIGHT_TOP + i)->EnableWindow(m_connected);
    }
    SetDlgItemText(IDC_AMBILIGHT_STATUS, m_connected
        ? L"Live preview - changes are saved automatically"
        : L"Open a video with MPC Video Renderer Ambilight.");
    m_loading = false;
}

void CAmbilightDialog::ApplyValue(size_t index, int value) {
    auto config = GetConfig();
    if (!config) { RefreshControls(); return; }
    if (SUCCEEDED(config->Flt_SetInt(AmbilightSettingDefinitions[index].key, value))) {
        config->Flt_SetBool("ambilightSave", true);
    }
}

void CAmbilightDialog::OnHScroll(UINT code, UINT position, CScrollBar* bar) {
    if (!m_loading && bar) {
        int index = bar->GetDlgCtrlID() - IDC_AMBILIGHT_SLIDER;
        if (index >= 0 && index < (int)AmbilightSliderCount) {
            int value = m_sliders[index].GetPos();
            m_loading = true;
            SetDlgItemInt(IDC_AMBILIGHT_VALUE + index, value, TRUE);
            m_loading = false;
            ApplyValue(index, value);
        }
    }
    __super::OnHScroll(code, position, bar);
}

BOOL CAmbilightDialog::OnCommand(WPARAM wParam, LPARAM lParam) {
    if (!m_loading) {
        const UINT id = LOWORD(wParam), notification = HIWORD(wParam);
        if (id >= IDC_AMBILIGHT_VALUE && id < IDC_AMBILIGHT_VALUE + AmbilightSliderCount) {
            const size_t index = id - IDC_AMBILIGHT_VALUE;
            if (notification == EN_CHANGE) {
                CString text; GetDlgItemText(id, text);
                wchar_t* end = nullptr;
                const long value = wcstol(text, &end, 10);
                const auto& definition = AmbilightSettingDefinitions[index];
                if (!text.IsEmpty() && end && !*end && value >= definition.minimum && value <= definition.maximum) {
                    m_sliders[index].SetPos(value);
                    ApplyValue(index, value);
                }
                return TRUE;
            }
            if (notification == EN_KILLFOCUS) { RefreshControls(); return TRUE; }
        }
        if (notification == BN_CLICKED) {
            if (id == IDC_AMBILIGHT_RESET) { Reset(); return TRUE; }
            if (id == IDCANCEL) { OnCancel(); return TRUE; }
            if (id == IDC_AMBILIGHT_ENABLED || (id >= IDC_AMBILIGHT_TOP && id <= IDC_AMBILIGHT_LEFT)) {
                auto config = GetConfig();
                if (config) {
                    if (id == IDC_AMBILIGHT_ENABLED) config->Flt_SetBool("ambilightEnabled", IsDlgButtonChecked(id) == BST_CHECKED);
                    else {
                        int mask = 0;
                        for (int i = 0; i < 4; ++i) if (IsDlgButtonChecked(IDC_AMBILIGHT_TOP + i) == BST_CHECKED) mask |= 1 << i;
                        config->Flt_SetInt("ambilightDirections", mask);
                    }
                    config->Flt_SetBool("ambilightSave", true);
                }
                return TRUE;
            }
        }
    }
    return __super::OnCommand(wParam, lParam);
}

void CAmbilightDialog::Reset() {
    auto config = GetConfig();
    if (!config) return;
    for (const auto& definition : AmbilightSettingDefinitions) config->Flt_SetInt(definition.key, definition.initial);
    config->Flt_SetBool("ambilightSave", true);
    RefreshControls();
}

void CAmbilightDialog::OnTimer(UINT_PTR event) {
    if (event == 1 && IsWindowVisible()) {
        // Reconnect after file/renderer changes, without retaining a closed graph.
        bool connected = !!GetConfig();
        if (connected != m_connected) RefreshControls();
    }
    __super::OnTimer(event);
}

void CAmbilightDialog::OnCancel() {
    // Values have already been previewed and saved; closing does not pause playback.
    ShowWindow(SW_HIDE);
}
