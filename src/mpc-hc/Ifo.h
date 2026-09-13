/*
 * (C) 2008-2013, 2016 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <cstdint>

#pragma pack(push, 1)


class CIfo
{
public:
    CIfo();
    ~CIfo();

    bool OpenFile(LPCTSTR strFile);
    bool SaveFile(LPCTSTR strFile);
    bool RemoveUOPs();

private:

    struct pgci_sub_t {
        uint16_t id     : 16;   // Language
        uint16_t        : 16;   // don't know
        uint32_t start  : 32;   // Start of unit
    };

    struct dvd_time_t {
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
        uint8_t frame_u;        // The two high bits are the frame rate.
    };

    typedef uint8_t command_data_t[8];
#define COMMAND_DATA_SIZE 8

    struct pgc_command_tbl_t {  // PGC Command Table
        uint16_t nr_of_pre;
        uint16_t nr_of_post;
        uint16_t nr_of_cell;
        uint16_t tbl_len;
        command_data_t* pre_commands;
        command_data_t* post_commands;
        command_data_t* cell_commands;
    };
#define PGC_COMMAND_TBL_SIZE 8

    typedef uint8_t  pgc_program_map_t;

    struct ifo_pgci_caddr_t {   // Cell Playback Information
        uint8_t chain_info : 8; // 0x5e 0xde(2 angles, no overlay), 0x5f 0x9f 0x9f 0xdf(4 angles overlay), 0x2 0xa 0x8(1 angle)
        uint8_t foo;            // parental control ??
        uint8_t still_time;
        uint8_t cell_cmd;

        dvd_time_t  playback_time;
        uint32_t    vobu_start;    // 1st vobu start
        uint32_t    ilvu_end;
        uint32_t    vobu_last_start;
        uint32_t    vobu_last_end;
    };

    struct ifo_pgc_cpos_t {         // Cell Position Information
        uint16_t vob_id     : 16;   // Video Object Identifier
        uint8_t  foo        : 8;    // Unknown
        uint8_t  cell_id    : 8;    // Cell Identifier
    };

#ifndef CLUT_T
#define CLUT_T

    struct clut_t {            // CLUT == Color LookUp Table
        uint8_t foo     : 8;    // UNKNOWN: 0x00?
        uint8_t y       : 8;
        uint8_t cr      : 8;
        uint8_t cb      : 8;
    };
#endif

    struct audio_status_t {            // Audio Status
#if BYTE_ORDER == BIG_ENDIAN
        uint8_t available   : 1;
        uint8_t link        : 7;
#else
        uint8_t link        : 7;
        uint8_t available   : 1;
#endif
        uint8_t foo     : 8;    // UNKNOWN
    };


    struct subp_status_t {            // Subpicture status
#if BYTE_ORDER == BIG_ENDIAN
        uint8_t available   : 1;
        uint8_t format4_3   : 7;
#else
        uint8_t format4_3   : 7;
        uint8_t available   : 1;
#endif
        uint8_t wide        : 8;
        uint8_t letter      : 8;
        uint8_t pan         : 8;
    };


    struct pgc_t {            // Program Chain Information
        uint16_t        zero_1;
        uint8_t         nr_of_programs;
        uint8_t         nr_of_cells;
        dvd_time_t      playback_time;
        uint32_t        prohibited_ops;    // New type?
        audio_status_t  audio_status[8];
        subp_status_t   subp_status[32];
        uint16_t        next_pgc_nr;
        uint16_t        prev_pgc_nr;
        uint16_t        goup_pgc_nr;
        uint8_t         still_time;
        uint8_t         pg_playback_mode;
        clut_t          clut[16];
        uint16_t        pgc_command_tbl_offset;
        uint16_t        pgc_program_map_offset;
        uint16_t        cell_playback_tbl_offset;
        uint16_t        cell_position_tbl_offset;
        pgc_command_tbl_t* pgc_command_tbl;
        pgc_program_map_t* pgc_program_map;
        ifo_pgci_caddr_t*  cell_playback_tbl;
        ifo_pgc_cpos_t*    cell_position_tbl;
    };
#define PGC_SIZE 236

    struct ifo_hdr_t {
        uint16_t num    : 16;   // number of entries
        uint16_t        : 16;   // UNKNOWN
        uint32_t len    : 32;   // length of table
    };

    struct lu_sub_t {
#if BYTE_ORDER == BIG_ENDIAN
        uint16_t foo1   : 4;    // don't know
        uint8_t menu_id : 4;    // 0=off, 3=root, 4=spu,
        // 5=audio, 6=angle, 7=ptt
#else
        uint8_t menu_id : 4;    // 0=off, 3=root, 4=spu,
        // 5=audio, 6=angle, 7=ptt
        uint16_t foo1   : 4;    // don't know
#endif
        uint16_t foo2   : 8;    // don't know
        uint16_t bar    : 16;   // don't know
        uint32_t start  : 32;   // Start of unit
    };


    BYTE*       m_pBuffer;
    DWORD       m_dwSize;

    ifo_hdr_t*  m_pPGCI;
    ifo_hdr_t*  m_pPGCIT;

    bool        IsVTS();
    bool        IsVMG();
    bool        IsInBuffer(const void* p, size_t len) const;

    pgc_t*      GetFirstPGC();
    pgc_t*      GetPGCI(const int title, const ifo_hdr_t* hdr);
    int         GetMiscPGCI(ifo_hdr_t* hdr, int title, uint8_t** ptr);
    void        RemovePgciUOPs(uint8_t* ptr);
};
#pragma pack(pop)
