#pragma once
#include <afxcmn.h>
#include "CMPCThemeToolTipCtrl.h"

class CMPCThemeSliderCtrl : public CSliderCtrl
{
public:
    CMPCThemeSliderCtrl();
    virtual ~CMPCThemeSliderCtrl();
    virtual void PreSubclassWindow();
    DECLARE_DYNAMIC(CMPCThemeSliderCtrl)
    DECLARE_MESSAGE_MAP()
protected:
    CBrush bgBrush;
    bool m_bDrag, m_bHover, lockToZero, jumpToClick;
    CMPCThemeToolTipCtrl themedToolTip;
public:
    afx_msg void OnNMCustomdraw(NMHDR* pNMHDR, LRESULT* pResult);
    void invalidateThumb();
    void checkHover(CPoint point);
    void SendScrollMsg(WORD wSBcode, WORD wHiWPARAM = 0);
    void SetLockToZero(bool enable = true) { lockToZero = enable; };
    // A horizontal slider that goes to where it was clicked rather than paging
    // towards it, which is what a seekbar has to do.
    void SetJumpToClick(bool enable = true) { jumpToClick = enable; };
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    void SetPosFromPoint(CPoint point);
    void EndJumpToClickDrag();
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    afx_msg void OnMouseLeave();
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
};

