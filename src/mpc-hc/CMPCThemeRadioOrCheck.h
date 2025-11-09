#pragma once
#include <afxwin.h>
class CMPCThemeRadioOrCheck : public CButton
{
public:
    CMPCThemeRadioOrCheck();
    virtual ~CMPCThemeRadioOrCheck();
    void PreSubclassWindow();
private:
    bool isHover;
    BOOL isAuto;
    CBrush bgBrush;
    enum RadioOrCheck {
        radioType,
        checkType,
        threeStateType,
        unknownType
    };
    RadioOrCheck buttonType;
protected:
    bool isFileDialogChild;
    DECLARE_DYNAMIC(CMPCThemeRadioOrCheck)
    DECLARE_MESSAGE_MAP()
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    void checkHover(UINT nFlags, CPoint point, bool invalidate = true);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
public:
    afx_msg void OnPaint();
    afx_msg void OnEnable(BOOL bEnable);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    void setFileDialogChild(bool set) { isFileDialogChild = set; };
    afx_msg void OnUpdateUIState(UINT nAction, UINT nUIElement);
};

