/*
 *	AutoComplete interfaces implementation.
 *
 *	Copyright 2004	Maxime Bellengé <maxime.bellenge@laposte.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/*
  Implemented:
  - ACO_AUTOAPPEND style
  - ACO_AUTOSUGGEST style
  - ACO_UPDOWNKEYDROPSLIST style

  - Handle pwzsRegKeyPath and pwszQuickComplete in Init

  TODO:
  - implement ACO_SEARCH style
  - implement ACO_FILTERPREFIXES style
  - implement ACO_USETAB style
  - implement ACO_RTLREADING style
  - implement ResetEnumerator
  - string compares should be case-insensitive, the content of the list should be sorted
  
 */
#include "config.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define COBJMACROS

#include "wine/debug.h"
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "undocshell.h"
#include "shlwapi.h"
#include "winerror.h"
#include "objbase.h"

#include "pidl.h"
#include "shlobj.h"
#include "shldisp.h"
#include "debughlp.h"
#include "shell32_main.h"

#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

typedef struct
{
    IAutoComplete2 IAutoComplete2_iface;
    IAutoCompleteDropDown IAutoCompleteDropDown_iface;
    LONG ref;
    BOOL initialized;
    BOOL enabled;
    HWND hwndEdit;
    HWND hwndListBox;
    WNDPROC wpOrigEditProc;
    WNDPROC wpOrigLBoxProc;
    WCHAR *txtbackup;
    WCHAR *quickComplete;
    IEnumString *enumstr;
    AUTOCOMPLETEOPTIONS options;
} IAutoCompleteImpl;

enum autoappend_flag
{
    autoappend_flag_yes,
    autoappend_flag_no,
    autoappend_flag_displayempty
};

static const WCHAR autocomplete_propertyW[] = {'W','i','n','e',' ','A','u','t','o',
                                               'c','o','m','p','l','e','t','e',' ',
                                               'c','o','n','t','r','o','l',0};

static inline IAutoCompleteImpl *impl_from_IAutoComplete2(IAutoComplete2 *iface)
{
    return CONTAINING_RECORD(iface, IAutoCompleteImpl, IAutoComplete2_iface);
}

static inline IAutoCompleteImpl *impl_from_IAutoCompleteDropDown(IAutoCompleteDropDown *iface)
{
    return CONTAINING_RECORD(iface, IAutoCompleteImpl, IAutoCompleteDropDown_iface);
}

static size_t format_quick_complete(WCHAR *dst, const WCHAR *qc, const WCHAR *str, size_t str_len)
{
    /* Replace the first %s directly without using snprintf, to avoid
       exploits since the format string can be retrieved from the registry */
    WCHAR *base = dst;
    UINT args = 0;
    while (*qc != '\0')
    {
        if (qc[0] == '%')
        {
            if (args < 1 && qc[1] == 's')
            {
                memcpy(dst, str, str_len * sizeof(WCHAR));
                dst += str_len;
                qc += 2;
                args++;
                continue;
            }
            qc += (qc[1] == '%');
        }
        *dst++ = *qc++;
    }
    *dst = '\0';
    return dst - base;
}

static void autoappend_str(IAutoCompleteImpl *ac, WCHAR *text, UINT len, WCHAR *str, HWND hwnd)
{
    WCHAR *tmp;
    size_t size;

    /* The character capitalization can be different,
       so merge text and str into a new string */
    size = len + strlenW(&str[len]) + 1;

    if ((tmp = heap_alloc(size * sizeof(*tmp))))
    {
        memcpy(tmp, text, len * sizeof(*tmp));
        memcpy(&tmp[len], &str[len], (size - len) * sizeof(*tmp));
    }
    else tmp = str;

    SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)tmp);
    SendMessageW(hwnd, EM_SETSEL, len, size - 1);
    if (tmp != str)
        heap_free(tmp);
}

static void autocomplete_text(IAutoCompleteImpl *ac, HWND hwnd, enum autoappend_flag flag)
{
    HRESULT hr;
    WCHAR *text;
    UINT cpt, size, len = SendMessageW(hwnd, WM_GETTEXTLENGTH, 0, 0);

    if (flag != autoappend_flag_displayempty && len == 0)
    {
        if (ac->options & ACO_AUTOSUGGEST)
            ShowWindow(ac->hwndListBox, SW_HIDE);
        return;
    }

    size = len + 1;
    if (!(text = heap_alloc(size * sizeof(WCHAR))))
        return;
    len = SendMessageW(hwnd, WM_GETTEXT, size, (LPARAM)text);
    if (len + 1 != size)
        text = heap_realloc(text, (len + 1) * sizeof(WCHAR));

    SendMessageW(ac->hwndListBox, LB_RESETCONTENT, 0, 0);

    /* Set txtbackup to point to text itself (which must not be released) */
    heap_free(ac->txtbackup);
    ac->txtbackup = text;

    IEnumString_Reset(ac->enumstr);
    for (cpt = 0;;)
    {
        LPOLESTR strs = NULL;
        ULONG fetched;

        hr = IEnumString_Next(ac->enumstr, 1, &strs, &fetched);
        if (hr != S_OK)
            break;

        if (!strncmpiW(text, strs, len))
        {
            if (cpt == 0 && flag == autoappend_flag_yes)
            {
                autoappend_str(ac, text, len, strs, hwnd);
                if (!(ac->options & ACO_AUTOSUGGEST))
                {
                    CoTaskMemFree(strs);
                    break;
                }
            }

            if (ac->options & ACO_AUTOSUGGEST)
                SendMessageW(ac->hwndListBox, LB_ADDSTRING, 0, (LPARAM)strs);

            cpt++;
        }

        CoTaskMemFree(strs);
    }

    if (ac->options & ACO_AUTOSUGGEST)
    {
        if (cpt)
        {
            RECT r;
            UINT height = SendMessageW(ac->hwndListBox, LB_GETITEMHEIGHT, 0, 0);
            SendMessageW(ac->hwndListBox, LB_CARETOFF, 0, 0);
            GetWindowRect(hwnd, &r);
            SetParent(ac->hwndListBox, HWND_DESKTOP);
            /* It seems that Windows XP displays 7 lines at most
               and otherwise displays a vertical scroll bar */
            SetWindowPos(ac->hwndListBox, HWND_TOP,
                         r.left, r.bottom + 1, r.right - r.left, height * min(cpt + 1, 7),
                         SWP_SHOWWINDOW );
        }
        else
            ShowWindow(ac->hwndListBox, SW_HIDE);
    }
}

static void destroy_autocomplete_object(IAutoCompleteImpl *ac)
{
    ac->hwndEdit = NULL;
    if (ac->hwndListBox)
        DestroyWindow(ac->hwndListBox);
    IAutoComplete2_Release(&ac->IAutoComplete2_iface);
}

/*
   Helper for ACEditSubclassProc
*/
static LRESULT ACEditSubclassProc_KeyDown(IAutoCompleteImpl *ac, HWND hwnd, UINT uMsg,
                                          WPARAM wParam, LPARAM lParam)
{
    switch (wParam)
    {
        case VK_RETURN:
            /* If quickComplete is set and control is pressed, replace the string */
            if (ac->quickComplete && (GetKeyState(VK_CONTROL) & 0x8000))
            {
                WCHAR *text, *buf;
                size_t sz;
                UINT len = SendMessageW(hwnd, WM_GETTEXTLENGTH, 0, 0);
                if (!(text = heap_alloc((len + 1) * sizeof(WCHAR))))
                    return 0;
                len = SendMessageW(hwnd, WM_GETTEXT, len + 1, (LPARAM)text);
                sz = strlenW(ac->quickComplete) + 1 + len;

                if ((buf = heap_alloc(sz * sizeof(WCHAR))))
                {
                    len = format_quick_complete(buf, ac->quickComplete, text, len);
                    SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)buf);
                    SendMessageW(hwnd, EM_SETSEL, 0, len);
                    heap_free(buf);
                }

                if (ac->options & ACO_AUTOSUGGEST)
                    ShowWindow(ac->hwndListBox, SW_HIDE);
                heap_free(text);
                return 0;
            }

            if (ac->options & ACO_AUTOSUGGEST)
                ShowWindow(ac->hwndListBox, SW_HIDE);
            break;
        case VK_UP:
        case VK_DOWN:
            /* Two cases here:
               - if the listbox is not visible and ACO_UPDOWNKEYDROPSLIST is
                 set, display it with all the entries, without selecting any
               - if the listbox is visible, change the selection
            */
            if (!(ac->options & ACO_AUTOSUGGEST))
                break;

            if (!IsWindowVisible(ac->hwndListBox))
            {
                if (ac->options & ACO_UPDOWNKEYDROPSLIST)
                {
                    autocomplete_text(ac, hwnd, autoappend_flag_displayempty);
                    return 0;
                }
            }
            else
            {
                INT count, sel;
                count = SendMessageW(ac->hwndListBox, LB_GETCOUNT, 0, 0);

                /* Change the selection */
                sel = SendMessageW(ac->hwndListBox, LB_GETCURSEL, 0, 0);
                if (wParam == VK_UP)
                    sel = ((sel - 1) < 0) ? count - 1 : sel - 1;
                else
                    sel = ((sel + 1) >= count) ? -1 : sel + 1;
                SendMessageW(ac->hwndListBox, LB_SETCURSEL, sel, 0);
                if (sel >= 0)
                {
                    WCHAR *msg;
                    UINT len;

                    len = SendMessageW(ac->hwndListBox, LB_GETTEXTLEN, sel, 0);
                    if (!(msg = heap_alloc((len + 1) * sizeof(WCHAR))))
                        return 0;
                    len = SendMessageW(ac->hwndListBox, LB_GETTEXT, sel, (LPARAM)msg);
                    SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)msg);
                    SendMessageW(hwnd, EM_SETSEL, len, len);
                    heap_free(msg);
                }
                else
                {
                    UINT len;
                    SendMessageW(hwnd, WM_SETTEXT, 0, (LPARAM)ac->txtbackup);
                    len = strlenW(ac->txtbackup);
                    SendMessageW(hwnd, EM_SETSEL, len, len);
                }
                return 0;
            }
            break;
        case VK_DELETE:
        {
            LRESULT ret = CallWindowProcW(ac->wpOrigEditProc, hwnd, uMsg, wParam, lParam);
            autocomplete_text(ac, hwnd, autoappend_flag_no);
            return ret;
        }
    }
    return CallWindowProcW(ac->wpOrigEditProc, hwnd, uMsg, wParam, lParam);
}

/*
  Window procedure for autocompletion
 */
static LRESULT APIENTRY ACEditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    IAutoCompleteImpl *This = GetPropW(hwnd, autocomplete_propertyW);
    LRESULT ret;

    if (!This->enabled) return CallWindowProcW(This->wpOrigEditProc, hwnd, uMsg, wParam, lParam);

    switch (uMsg)
    {
        case CB_SHOWDROPDOWN:
            if (This->options & ACO_AUTOSUGGEST)
                ShowWindow(This->hwndListBox, SW_HIDE);
            break;
        case WM_KILLFOCUS:
            if ((This->options & ACO_AUTOSUGGEST) && ((HWND)wParam != This->hwndListBox))
            {
                ShowWindow(This->hwndListBox, SW_HIDE);
            }
            return CallWindowProcW(This->wpOrigEditProc, hwnd, uMsg, wParam, lParam);
        case WM_KEYDOWN:
            return ACEditSubclassProc_KeyDown(This, hwnd, uMsg, wParam, lParam);
        case WM_CHAR:
        case WM_UNICHAR:
            ret = CallWindowProcW(This->wpOrigEditProc, hwnd, uMsg, wParam, lParam);
            autocomplete_text(This, hwnd, (This->options & ACO_AUTOAPPEND) && wParam >= ' '
                                          ? autoappend_flag_yes : autoappend_flag_no);
            return ret;
        case WM_DESTROY:
        {
            WNDPROC proc = This->wpOrigEditProc;

            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)proc);
            RemovePropW(hwnd, autocomplete_propertyW);
            destroy_autocomplete_object(This);
            return CallWindowProcW(proc, hwnd, uMsg, wParam, lParam);
        }
        default:
            return CallWindowProcW(This->wpOrigEditProc, hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

static LRESULT APIENTRY ACLBoxSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    IAutoCompleteImpl *This = (IAutoCompleteImpl *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    WCHAR *msg;
    int sel, len;

    switch (uMsg) {
        case WM_MOUSEMOVE:
            sel = SendMessageW(hwnd, LB_ITEMFROMPOINT, 0, lParam);
            SendMessageW(hwnd, LB_SETCURSEL, sel, 0);
            break;
        case WM_LBUTTONDOWN:
            sel = SendMessageW(hwnd, LB_GETCURSEL, 0, 0);
            if (sel < 0)
                break;
            len = SendMessageW(This->hwndListBox, LB_GETTEXTLEN, sel, 0);
            if (!(msg = heap_alloc((len + 1) * sizeof(WCHAR))))
                break;
            len = SendMessageW(hwnd, LB_GETTEXT, sel, (LPARAM)msg);
            SendMessageW(This->hwndEdit, WM_SETTEXT, 0, (LPARAM)msg);
            SendMessageW(This->hwndEdit, EM_SETSEL, 0, len);
            ShowWindow(hwnd, SW_HIDE);
            heap_free(msg);
            break;
        default:
            return CallWindowProcW(This->wpOrigLBoxProc, hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

static void create_listbox(IAutoCompleteImpl *This)
{
    HWND hwndParent;

    hwndParent = GetParent(This->hwndEdit);

    /* FIXME : The listbox should be resizable with the mouse. WS_THICKFRAME looks ugly */
    This->hwndListBox = CreateWindowExW(0, WC_LISTBOXW, NULL,
                                    WS_BORDER | WS_CHILD | WS_VSCROLL | LBS_HASSTRINGS | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                    hwndParent, NULL, shell32_hInstance, NULL );

    if (This->hwndListBox) {
        This->wpOrigLBoxProc = (WNDPROC) SetWindowLongPtrW( This->hwndListBox, GWLP_WNDPROC, (LONG_PTR) ACLBoxSubclassProc);
        SetWindowLongPtrW( This->hwndListBox, GWLP_USERDATA, (LONG_PTR)This);
    }
    else
        This->options &= ~ACO_AUTOSUGGEST;
}

/**************************************************************************
 *  AutoComplete_QueryInterface
 */
static HRESULT WINAPI IAutoComplete2_fnQueryInterface(
    IAutoComplete2 * iface,
    REFIID riid,
    LPVOID *ppvObj)
{
    IAutoCompleteImpl *This = impl_from_IAutoComplete2(iface);

    TRACE("(%p)->(IID:%s,%p)\n", This, shdebugstr_guid(riid), ppvObj);
    *ppvObj = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IAutoComplete) ||
        IsEqualIID(riid, &IID_IAutoComplete2))
    {
        *ppvObj = &This->IAutoComplete2_iface;
    }
    else if (IsEqualIID(riid, &IID_IAutoCompleteDropDown))
    {
        *ppvObj = &This->IAutoCompleteDropDown_iface;
    }

    if (*ppvObj)
    {
	IUnknown_AddRef((IUnknown*)*ppvObj);
	TRACE("-- Interface: (%p)->(%p)\n", ppvObj, *ppvObj);
	return S_OK;
    }
    WARN("unsupported interface: %s\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

/******************************************************************************
 * IAutoComplete2_fnAddRef
 */
static ULONG WINAPI IAutoComplete2_fnAddRef(
	IAutoComplete2 * iface)
{
    IAutoCompleteImpl *This = impl_from_IAutoComplete2(iface);
    ULONG refCount = InterlockedIncrement(&This->ref);

    TRACE("(%p)->(%u)\n", This, refCount - 1);

    return refCount;
}

/******************************************************************************
 * IAutoComplete2_fnRelease
 */
static ULONG WINAPI IAutoComplete2_fnRelease(
	IAutoComplete2 * iface)
{
    IAutoCompleteImpl *This = impl_from_IAutoComplete2(iface);
    ULONG refCount = InterlockedDecrement(&This->ref);

    TRACE("(%p)->(%u)\n", This, refCount + 1);

    if (!refCount) {
        TRACE("destroying IAutoComplete(%p)\n", This);
        heap_free(This->quickComplete);
        heap_free(This->txtbackup);
        if (This->enumstr)
            IEnumString_Release(This->enumstr);
        heap_free(This);
    }
    return refCount;
}

/******************************************************************************
 * IAutoComplete2_fnEnable
 */
static HRESULT WINAPI IAutoComplete2_fnEnable(
    IAutoComplete2 * iface,
    BOOL fEnable)
{
    IAutoCompleteImpl *This = impl_from_IAutoComplete2(iface);
    HRESULT hr = S_OK;

    TRACE("(%p)->(%s)\n", This, (fEnable)?"true":"false");

    This->enabled = fEnable;

    return hr;
}

/******************************************************************************
 * IAutoComplete2_fnInit
 */
static HRESULT WINAPI IAutoComplete2_fnInit(
    IAutoComplete2 * iface,
    HWND hwndEdit,
    IUnknown *punkACL,
    LPCOLESTR pwzsRegKeyPath,
    LPCOLESTR pwszQuickComplete)
{
    IAutoCompleteImpl *prev, *This = impl_from_IAutoComplete2(iface);

    TRACE("(%p)->(%p, %p, %s, %s)\n",
	  This, hwndEdit, punkACL, debugstr_w(pwzsRegKeyPath), debugstr_w(pwszQuickComplete));

    if (This->options & ACO_SEARCH) FIXME(" ACO_SEARCH not supported\n");
    if (This->options & ACO_FILTERPREFIXES) FIXME(" ACO_FILTERPREFIXES not supported\n");
    if (This->options & ACO_USETAB) FIXME(" ACO_USETAB not supported\n");
    if (This->options & ACO_RTLREADING) FIXME(" ACO_RTLREADING not supported\n");

    if (!hwndEdit || !punkACL)
        return E_INVALIDARG;

    if (This->initialized)
    {
        WARN("Autocompletion object is already initialized\n");
        /* This->hwndEdit is set to NULL when the edit window is destroyed. */
        return This->hwndEdit ? E_FAIL : E_UNEXPECTED;
    }

    if (FAILED (IUnknown_QueryInterface (punkACL, &IID_IEnumString, (LPVOID*)&This->enumstr))) {
        WARN("No IEnumString interface\n");
        return E_NOINTERFACE;
    }

    This->initialized = TRUE;
    This->hwndEdit = hwndEdit;

    /* If another AutoComplete object was previously assigned to this edit control,
       release it but keep the same callback on the control, to avoid an infinite
       recursive loop in ACEditSubclassProc while the property is set to this object */
    prev = GetPropW(hwndEdit, autocomplete_propertyW);
    SetPropW(hwndEdit, autocomplete_propertyW, This);

    if (prev && prev->initialized) {
        This->wpOrigEditProc = prev->wpOrigEditProc;
        destroy_autocomplete_object(prev);
    }
    else
        This->wpOrigEditProc = (WNDPROC) SetWindowLongPtrW(hwndEdit, GWLP_WNDPROC, (LONG_PTR) ACEditSubclassProc);

    /* Keep at least one reference to the object until the edit window is destroyed */
    IAutoComplete2_AddRef(&This->IAutoComplete2_iface);

    if (This->options & ACO_AUTOSUGGEST)
        create_listbox(This);

    if (pwzsRegKeyPath)
    {
        static const HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
        WCHAR *key, *value;
        DWORD type, sz;
        BYTE *qc;
        HKEY hKey;
        LSTATUS res;
        size_t len;
        UINT i;

        /* pwszRegKeyPath contains the key as well as the value, so split it */
        value = strrchrW(pwzsRegKeyPath, '\\');
        len = value - pwzsRegKeyPath;

        if (value && (key = heap_alloc((len+1) * sizeof(*key))) != NULL)
        {
            memcpy(key, pwzsRegKeyPath, len * sizeof(*key));
            key[len] = '\0';
            value++;

            for (i = 0; i < ARRAY_SIZE(roots); i++)
            {
                if (RegOpenKeyExW(roots[i], key, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
                    continue;
                sz = MAX_PATH * sizeof(WCHAR);

                while ((qc = heap_alloc(sz)) != NULL)
                {
                    res = RegQueryValueExW(hKey, value, NULL, &type, qc, &sz);
                    if (res == ERROR_SUCCESS && type == REG_SZ)
                    {
                        This->quickComplete = heap_realloc(qc, sz);
                        i = ARRAY_SIZE(roots);
                        break;
                    }
                    heap_free(qc);
                    if (res != ERROR_MORE_DATA || type != REG_SZ)
                        break;
                }
                RegCloseKey(hKey);
            }
            heap_free(key);
        }
    }

    if (!This->quickComplete && pwszQuickComplete)
    {
        size_t len = strlenW(pwszQuickComplete)+1;
        if ((This->quickComplete = heap_alloc(len * sizeof(WCHAR))) != NULL)
            memcpy(This->quickComplete, pwszQuickComplete, len * sizeof(WCHAR));
    }

    return S_OK;
}

/**************************************************************************
 *  IAutoComplete2_fnGetOptions
 */
static HRESULT WINAPI IAutoComplete2_fnGetOptions(
    IAutoComplete2 * iface,
    DWORD *pdwFlag)
{
    IAutoCompleteImpl *This = impl_from_IAutoComplete2(iface);
    HRESULT hr = S_OK;

    TRACE("(%p) -> (%p)\n", This, pdwFlag);

    *pdwFlag = This->options;

    return hr;
}

/**************************************************************************
 *  IAutoComplete2_fnSetOptions
 */
static HRESULT WINAPI IAutoComplete2_fnSetOptions(
    IAutoComplete2 * iface,
    DWORD dwFlag)
{
    IAutoCompleteImpl *This = impl_from_IAutoComplete2(iface);
    HRESULT hr = S_OK;

    TRACE("(%p) -> (0x%x)\n", This, dwFlag);

    This->options = dwFlag;

    if ((This->options & ACO_AUTOSUGGEST) && This->hwndEdit && !This->hwndListBox)
        create_listbox(This);

    return hr;
}

/**************************************************************************
 *  IAutoComplete2 VTable
 */
static const IAutoComplete2Vtbl acvt =
{
    IAutoComplete2_fnQueryInterface,
    IAutoComplete2_fnAddRef,
    IAutoComplete2_fnRelease,
    IAutoComplete2_fnInit,
    IAutoComplete2_fnEnable,
    /* IAutoComplete2 */
    IAutoComplete2_fnSetOptions,
    IAutoComplete2_fnGetOptions,
};


static HRESULT WINAPI IAutoCompleteDropDown_fnQueryInterface(IAutoCompleteDropDown *iface,
            REFIID riid, LPVOID *ppvObj)
{
    IAutoCompleteImpl *This = impl_from_IAutoCompleteDropDown(iface);
    return IAutoComplete2_QueryInterface(&This->IAutoComplete2_iface, riid, ppvObj);
}

static ULONG WINAPI IAutoCompleteDropDown_fnAddRef(IAutoCompleteDropDown *iface)
{
    IAutoCompleteImpl *This = impl_from_IAutoCompleteDropDown(iface);
    return IAutoComplete2_AddRef(&This->IAutoComplete2_iface);
}

static ULONG WINAPI IAutoCompleteDropDown_fnRelease(IAutoCompleteDropDown *iface)
{
    IAutoCompleteImpl *This = impl_from_IAutoCompleteDropDown(iface);
    return IAutoComplete2_Release(&This->IAutoComplete2_iface);
}

/**************************************************************************
 *  IAutoCompleteDropDown_fnGetDropDownStatus
 */
static HRESULT WINAPI IAutoCompleteDropDown_fnGetDropDownStatus(
    IAutoCompleteDropDown *iface,
    DWORD *pdwFlags,
    LPWSTR *ppwszString)
{
    IAutoCompleteImpl *This = impl_from_IAutoCompleteDropDown(iface);
    BOOL dropped;

    TRACE("(%p) -> (%p, %p)\n", This, pdwFlags, ppwszString);

    dropped = IsWindowVisible(This->hwndListBox);

    if (pdwFlags)
        *pdwFlags = (dropped ? ACDD_VISIBLE : 0);

    if (ppwszString) {
        if (dropped) {
            int sel;

            sel = SendMessageW(This->hwndListBox, LB_GETCURSEL, 0, 0);
            if (sel >= 0)
            {
                DWORD len;

                len = SendMessageW(This->hwndListBox, LB_GETTEXTLEN, sel, 0);
                *ppwszString = CoTaskMemAlloc((len+1)*sizeof(WCHAR));
                SendMessageW(This->hwndListBox, LB_GETTEXT, sel, (LPARAM)*ppwszString);
            }
            else
                *ppwszString = NULL;
        }
        else
            *ppwszString = NULL;
    }

    return S_OK;
}

/**************************************************************************
 *  IAutoCompleteDropDown_fnResetEnumarator
 */
static HRESULT WINAPI IAutoCompleteDropDown_fnResetEnumerator(
    IAutoCompleteDropDown *iface)
{
    IAutoCompleteImpl *This = impl_from_IAutoCompleteDropDown(iface);

    FIXME("(%p): stub\n", This);

    return E_NOTIMPL;
}

/**************************************************************************
 *  IAutoCompleteDropDown VTable
 */
static const IAutoCompleteDropDownVtbl acdropdownvt =
{
    IAutoCompleteDropDown_fnQueryInterface,
    IAutoCompleteDropDown_fnAddRef,
    IAutoCompleteDropDown_fnRelease,
    IAutoCompleteDropDown_fnGetDropDownStatus,
    IAutoCompleteDropDown_fnResetEnumerator,
};

/**************************************************************************
 *  IAutoComplete_Constructor
 */
HRESULT WINAPI IAutoComplete_Constructor(IUnknown * pUnkOuter, REFIID riid, LPVOID * ppv)
{
    IAutoCompleteImpl *lpac;
    HRESULT hr;

    if (pUnkOuter && !IsEqualIID (riid, &IID_IUnknown))
        return CLASS_E_NOAGGREGATION;

    lpac = heap_alloc_zero(sizeof(*lpac));
    if (!lpac)
        return E_OUTOFMEMORY;

    lpac->ref = 1;
    lpac->IAutoComplete2_iface.lpVtbl = &acvt;
    lpac->IAutoCompleteDropDown_iface.lpVtbl = &acdropdownvt;
    lpac->enabled = TRUE;
    lpac->options = ACO_AUTOAPPEND;

    hr = IAutoComplete2_QueryInterface(&lpac->IAutoComplete2_iface, riid, ppv);
    IAutoComplete2_Release(&lpac->IAutoComplete2_iface);

    TRACE("-- (%p)->\n",lpac);

    return hr;
}
