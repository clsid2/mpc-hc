#pragma once
#include "CMPCThemeDialog.h"
#include "CMPCThemeSliderCtrl.h"
#include <FilterInterfaces.h>
#include <AmbilightSettings.h>
#include <functional>
#include "resource.h"

class CAmbilightDialog : public CMPCThemeDialog {
public:
    using ConfigProvider = std::function<CComPtr<IExFilterConfig>()>;
    CAmbilightDialog(CWnd* parent, ConfigProvider provider);
    ~CAmbilightDialog();
    void RefreshControls();
protected:
    BOOL OnInitDialog() override;
    BOOL OnCommand(WPARAM wParam, LPARAM lParam) override;
    void OnOK() override {}
    void OnCancel() override;
    afx_msg void OnHScroll(UINT code, UINT position, CScrollBar* bar);
    afx_msg void OnTimer(UINT_PTR event);
    DECLARE_MESSAGE_MAP()
private:
    ConfigProvider m_provider;
    CMPCThemeSliderCtrl m_sliders[AmbilightSliderCount];
    bool m_loading = false;
    bool m_connected = false;
    CComPtr<IExFilterConfig> GetConfig();
    void ApplyValue(size_t index, int value);
    void Reset();
};
