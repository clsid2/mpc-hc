; (C) 2009-2017 see Authors.txt
;
; This file is part of MPC-HC.
;
; MPC-HC is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 3 of the License, or
; (at your option) any later version.
;
; MPC-HC is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>.


; Do NOT translate your language's name
; Do NOT translate [name]/[ver]
; Do NOT change the langid  <http://msdn.microsoft.com/en-us/goglobal/bb964664.aspx?>
; Do NOT change the file encoding; it must be UTF-8 Signature
; Keep the translations close to the English strings
; comp=component, msg=Message, tsk=Task


[Messages]
; English
WelcomeLabel1=[name/ver]
en.WelcomeLabel2=This will install [name] on your computer.%n%nIt is recommended that you close all other applications before continuing.
en.WinVersionTooLowError=[name] requires Windows 7 or newer to run.


[CustomMessages]
; English
en.langid=00000000
en.comp_mpciconlib=Icon Library
en.comp_mpcresources=Translations
en.msg_DeleteSettings=Do you also want to delete MPC-HC settings?%n%nIf you plan on installing MPC-HC again then you do not have to delete them.
en.msg_simd_sse2=This build of MPC-HC requires a CPU with SSE2 extension support.%n%nYour CPU does not have those capabilities.
en.tsk_AllUsers=For all users
en.tsk_CurrentUser=For the current user only
en.tsk_Other=Other tasks:
en.tsk_ResetSettings=Reset settings
en.types_DefaultInstallation=Default installation
en.types_CustomInstallation=Custom installation


#if localize == "true"
  #include "custom_messages_translated.iss"
#endif
