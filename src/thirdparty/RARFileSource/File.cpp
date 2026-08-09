/*
 * Copyright (C) 2008-2012, OctaneSnail <os@v12pwr.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <windows.h>
#include <streams.h>

#include "File.h"
#include "Utils.h"
#include "unrar/rar.hpp"

#include <vector>

size_t UnstoreFile(ComprDataIO &DataIO, std::vector<byte>& output) {
    size_t bufSize = output.size() > File::CopyBufferSize() ? File::CopyBufferSize() : output.size();
    std::vector<byte> Buffer(bufSize);
    size_t totalRead = 0;
    while (totalRead < output.size()) {
        int ReadSize = DataIO.UnpRead(&Buffer[0], Buffer.size());
        if (ReadSize <= 0)
            break;
        if (ReadSize > 0) {
            if (ReadSize + totalRead > output.size()) { //even though it never happens, ensure memcpy cannot copy beyond size of output
                ReadSize = output.size() - totalRead;
            }
            memcpy(&output[totalRead], &Buffer[0], ReadSize);
            totalRead += ReadSize;
        }
    }
    return totalRead;
}


bool ExtractCurrentFile(CmdExtract *cmdExtract, Archive &Arc, int64 extractStartOffset, int64 extractEndOffset, BYTE* pBuffer, size_t &totalRead)
{
  wchar Command='E';

  ComprDataIO &DataIO = cmdExtract->DataIO;
  Unpack *Unp = cmdExtract->Unp;

  if (Arc.FileHead.HeadSize ==0)
    if (DataIO.UnpVolume)
    {
      if (!MergeArchive(Arc,&DataIO,false,Command))
      {
        ErrHandler.SetErrorCode(RARX_WARNING);
        return false;
      }
    }
    else
      return false;

  HEADER_TYPE HeaderType=Arc.GetHeaderType();
  if (HeaderType!=HEAD_FILE)
  {
    return false;
  }

  if (Arc.FileHead.PackSize<0)
    Arc.FileHead.PackSize=0;
  if (Arc.FileHead.UnpSize<0)
    Arc.FileHead.UnpSize=0;

  std::wstring ArcFileName;
  ConvertPath(&Arc.FileHead.FileName,&ArcFileName);

  DataIO.UnpVolume=Arc.FileHead.SplitAfter;
  DataIO.NextVolumeMissing=false;

  Arc.Seek(Arc.NextBlockPos - Arc.FileHead.PackSize, SEEK_SET);

  if (!cmdExtract->CheckUnpVer(Arc,ArcFileName))
  {
      return false;
  }

  if (Arc.FileHead.Encrypted)
  {
      return false;
  }

  File CurFile;

  DataIO.CurUnpRead=0;
  DataIO.CurUnpWrite=0;
  DataIO.UnpHash.Init(Arc.FileHead.FileHash.Type,1);
  DataIO.PackedDataHash.Init(Arc.FileHead.FileHash.Type,1);
  DataIO.SetPackedSizeToRead(Arc.FileHead.PackSize);
  DataIO.SetFiles(&Arc,&CurFile);
  DataIO.SetTestMode(true);
  DataIO.SetSkipUnpCRC(false);

  totalRead = 0;
  if (!Arc.FileHead.SplitBefore)
    if (Arc.FileHead.Method==0) {
      int64 lastByte = Arc.FileHead.PackSize - 1;
      while (extractStartOffset > lastByte) {
        if (!MergeArchive(Arc, &DataIO, false, Command)) return false;
        lastByte += Arc.FileHead.PackSize;
      }
      int64 curOffset = extractStartOffset;
      while (curOffset < extractEndOffset) {
        Arc.Seek(Arc.NextBlockPos + curOffset - (lastByte + 1), SEEK_SET);
        size_t readSize;
        if (lastByte > extractEndOffset) {
          readSize = extractEndOffset - curOffset + 1;
        } else {
          readSize = lastByte - curOffset + 1;
        }
        std::vector<byte> output(readSize);
        size_t bytesRead = UnstoreFile(DataIO, output);
        if (bytesRead < 0) return false;
        memcpy(&pBuffer[curOffset-extractStartOffset], &output[0], bytesRead);
        curOffset += bytesRead;
        totalRead += bytesRead;
        if (curOffset > lastByte) {
            if (!MergeArchive(Arc, &DataIO, false, Command)) break; //no more volumes.  we have read what we can
            lastByte += Arc.FileHead.PackSize;
        }
      }
    }
    else
    {
      Unp->Init(Arc.FileHead.WinSize,Arc.FileHead.Solid);
      Unp->SetDestSize(Arc.FileHead.UnpSize);
      if (Arc.Format!=RARFMT50 && Arc.FileHead.UnpVer<=15)
        Unp->DoUnpack(15,cmdExtract->FileCount>1 && Arc.Solid);
      else
        Unp->DoUnpack(Arc.FileHead.UnpVer,Arc.FileHead.Solid);
    }
 
  if (DataIO.NextVolumeMissing)
    return false;
  return true;
}

CRFSFile::ReadThread::ReadThread(CRFSFile* file, LONGLONG llPosition, DWORD lLength, BYTE* pBuffer) {
    this->file = file;
    this->llPosition = llPosition;
    this->lLength = lLength;
    this->pBuffer = pBuffer;
    this->read = 0;
}

DWORD CRFSFile::ReadThread::ThreadStart() {
    if (file) {
        return file->SyncRead(llPosition, lLength, pBuffer, &read);
    } else {
        return S_FALSE;
    }
}

DWORD WINAPI CRFSFile::ReadThread::ThreadStartStatic(void *param) {
    ReadThread* t = (ReadThread*)param;
    if (t) {
        return t->ThreadStart();
    } else {
        return S_FALSE;
    }
}

HRESULT CRFSFile::SyncRead(LONGLONG llPosition, DWORD lLength, BYTE* pBuffer, LONG* cbActual) {
    Archive rarArchive;
    rarArchive.SetExceptions(false);
    CommandData cdata;
    cdata.Test = true;
    cdata.FileArgs.AddString(filename);
    cdata.Threads = 1; 
    cdata.Callback = NULL;
    CmdExtract cmd(&cdata);

    if (!rarArchive.Open(rarFilename)) {
        ErrorMsg(GetLastError(), L"CRFSOutputPin::SyncRead - Archive Open");
        return E_FAIL;
    }
    
    if (!rarArchive.IsArchive(false)) {
        ErrorMsg(GetLastError(), L"CRFSOutputPin::SyncRead - IsArchive");
        return E_FAIL;
    }
    rarArchive.Seek(startingBlockPos, SEEK_SET);
    if (0 == rarArchive.SearchBlock(HEAD_FILE)) {
        ErrorMsg(GetLastError(), L"CRFSOutputPin::SyncRead - SearchBlock");
        return E_FAIL;
    }
    cmd.ExtractArchiveInit(rarArchive);

    size_t totalRead;
    if (ExtractCurrentFile(&cmd, rarArchive, llPosition, llPosition + lLength - 1, pBuffer, totalRead)) {
        if (cbActual)
            *cbActual = totalRead;
        return S_OK;
    } else {
        return E_FAIL;
    }

}


