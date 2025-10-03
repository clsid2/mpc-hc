#include "stdafx.h"
#include "CMPCThemeButton.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "mplayerc.h"

CMPCThemeButton::CMPCThemeButton()
{
    drawShield = false;
}

CMPCThemeButton::~CMPCThemeButton()
{
}

IMPLEMENT_DYNAMIC(CMPCThemeButton, CButton)
BEGIN_MESSAGE_MAP(CMPCThemeButton, CButton)
    ON_WM_SETFONT()
    ON_WM_GETFONT()
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CMPCThemeButton::OnNMCustomdraw)
    ON_MESSAGE(BCM_SETSHIELD, &setShieldIcon)
END_MESSAGE_MAP()

LRESULT CMPCThemeButton::setShieldIcon(WPARAM wParam, LPARAM lParam)
{
    drawShield = (BOOL)lParam;
    return 1; //pass it along
}

void CMPCThemeButton::drawButtonBase(CDC* pDC, CRect rect, CString strText, bool selected, bool highLighted, bool focused, bool disabled, bool thin, bool shield, HWND accelWindow)
{
    CBrush fb, fb2;
    fb.CreateSolidBrush(CMPCTheme::ButtonBorderOuterColor);

    if (!thin) { //some small buttons look very ugly with the full border.  make up our own solution
        pDC->FrameRect(rect, &fb);
        rect.DeflateRect(1, 1);
    }
    COLORREF bg = CMPCTheme::ButtonFillColor, dottedClr = CMPCTheme::ButtonBorderKBFocusColor;

    if (selected) {//mouse down
        fb2.CreateSolidBrush(CMPCTheme::ButtonBorderInnerColor);
        bg = CMPCTheme::ButtonFillSelectedColor;
        dottedClr = CMPCTheme::ButtonBorderSelectedKBFocusColor;
    } else if (highLighted) {
        fb2.CreateSolidBrush(CMPCTheme::ButtonBorderInnerColor);
        bg = CMPCTheme::ButtonFillHoverColor;
        dottedClr = CMPCTheme::ButtonBorderHoverKBFocusColor;
    } else if (focused) {
        fb2.CreateSolidBrush(CMPCTheme::ButtonBorderInnerFocusedColor);
    } else {
        fb2.CreateSolidBrush(CMPCTheme::ButtonBorderInnerColor);
    }

    pDC->FrameRect(rect, &fb2);
    rect.DeflateRect(1, 1);
    pDC->FillSolidRect(rect, bg);


    if (focused) {
        rect.DeflateRect(1, 1);
        COLORREF oldTextFGColor = pDC->SetTextColor(dottedClr);
        COLORREF oldBGColor = pDC->SetBkColor(bg);
        CBrush* dotted = pDC->GetHalftoneBrush();
        pDC->FrameRect(rect, dotted);
        DeleteObject(dotted);
        pDC->SetTextColor(oldTextFGColor);
        pDC->SetBkColor(oldBGColor);
    }

    if (!strText.IsEmpty()) {
        int nMode = pDC->SetBkMode(TRANSPARENT);

        COLORREF oldTextFGColor;
        if (disabled) {
            oldTextFGColor = pDC->SetTextColor(CMPCTheme::ButtonDisabledFGColor);
        } else {
            oldTextFGColor = pDC->SetTextColor(CMPCTheme::TextFGColor);
        }

        UINT format = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
        if (shield) {
            int iconsize = MulDiv(pDC->GetDeviceCaps(LOGPIXELSX), 1, 6);
            int shieldY = rect.top + (rect.Height() - iconsize) / 2 + 1;
            CRect centerRect = rect;
            pDC->DrawTextW(strText, rect, format | DT_CALCRECT);
            rect.top = centerRect.top;
            rect.bottom = centerRect.bottom;
            rect.OffsetRect((centerRect.Width() - rect.Width() + iconsize) / 2, 0);
            int shieldX = rect.left - iconsize - 1;
            HICON hShieldIcon = (HICON)LoadImage(0, IDI_SHIELD, IMAGE_ICON, iconsize, iconsize, LR_SHARED);
            if (hShieldIcon) {
                DrawIconEx(pDC->GetSafeHdc(), shieldX, shieldY, hShieldIcon, iconsize, iconsize, 0, NULL, DI_NORMAL);
            }
        }

        if (accelWindow && (::SendMessage(accelWindow, WM_QUERYUISTATE, 0, 0) & UISF_HIDEACCEL) != 0) {
            format |= DT_HIDEPREFIX;
        }
        pDC->DrawTextW(strText, rect, format);

        pDC->SetTextColor(oldTextFGColor);
        pDC->SetBkMode(nMode);
    }
    fb.DeleteObject();
    fb2.DeleteObject();
}

#define max(a,b)            (((a) > (b)) ? (a) : (b))
void CMPCThemeButton::drawButton(HDC hdc, CRect rect, UINT state)
{
    CDC* pDC = CDC::FromHandle(hdc);

    CString strText;
    GetWindowText(strText);
    bool selected = CDIS_SELECTED == (state & CDIS_SELECTED);
    bool focused = CDIS_FOCUS == (state & CDIS_FOCUS);
    bool disabled = CDIS_DISABLED == (state & CDIS_DISABLED);
    bool hot = CDIS_HOT == (state & CDIS_HOT);

    BUTTON_IMAGELIST imgList;
    GetImageList(&imgList);
    CImageList* images = CImageList::FromHandlePermanent(imgList.himl);
    //bool thin = (images != nullptr); //thin borders for image buttons
    bool thin = true;


    drawButtonBase(pDC, rect, strText, selected, hot, focused, disabled, thin, drawShield, m_hWnd);

    int imageIndex = 0; //Normal
    if (disabled) {
        imageIndex = 1;
    }

    if (images != nullptr) { //assume centered
        IMAGEINFO ii;
        if (images->GetImageCount() <= imageIndex) {
            imageIndex = 0;
        }
        images->GetImageInfo(imageIndex, &ii);
        int width = ii.rcImage.right - ii.rcImage.left;
        int height = ii.rcImage.bottom - ii.rcImage.top;
        rect.DeflateRect((rect.Width() - width) / 2, max(0, (rect.Height() - height) / 2));
        images->Draw(pDC, imageIndex, rect.TopLeft(), ILD_NORMAL);
    }
}

void CMPCThemeButton::OnNMCustomdraw(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
    *pResult = CDRF_DODEFAULT;
    if (AppIsThemeLoaded()) {
        if (pNMCD->dwDrawStage == CDDS_PREERASE) {
            drawButton(pNMCD->hdc, pNMCD->rc, pNMCD->uItemState);
            *pResult = CDRF_SKIPDEFAULT;
        }
    }
}
void CMPCThemeButton::OnSetFont(CFont* pFont, BOOL bRedraw)
{
    Default(); //bypass the MFCButton font impl since we don't always draw this button ourselves (classic mode)
}

HFONT CMPCThemeButton::OnGetFont()
{
    return (HFONT)Default();
}
