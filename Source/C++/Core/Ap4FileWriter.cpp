/*****************************************************************
|
|    AP4 - File Writer
|
|    Copyright 2002-2006 Gilles Boccon-Gibod & Julien Boeuf
|
|
|    This file is part of Bento4/AP4 (MP4 Atom Processing Library).
|
|    Unless you have obtained Bento4 under a difference license,
|    this version of Bento4 is Bento4|GPL.
|    Bento4|GPL is free software; you can redistribute it and/or modify
|    it under the terms of the GNU General Public License as published by
|    the Free Software Foundation; either version 2, or (at your option)
|    any later version.
|
|    Bento4|GPL is distributed in the hope that it will be useful,
|    but WITHOUT ANY WARRANTY; without even the implied warranty of
|    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
|    GNU General Public License for more details.
|
|    You should have received a copy of the GNU General Public License
|    along with Bento4|GPL; see the file COPYING.  If not, write to the
|    Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
|    02111-1307, USA.
|
 ****************************************************************/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "Ap4MoovAtom.h"
#include "Ap4FileWriter.h"
#include "Ap4Movie.h"
#include "Ap4File.h"
#include "Ap4TrakAtom.h"
#include "Ap4Track.h"
#include "Ap4Sample.h"
#include "Ap4DataBuffer.h"
#include "Ap4FtypAtom.h"

/*----------------------------------------------------------------------
|   AP4_FileWriter::AP4_FileWriter
+---------------------------------------------------------------------*/
AP4_FileWriter::AP4_FileWriter(AP4_File& file) : m_File(file)
{
}

/*----------------------------------------------------------------------
|   AP4_FileWriter::~AP4_FileWriter
+---------------------------------------------------------------------*/
AP4_FileWriter::~AP4_FileWriter()
{
}

/*----------------------------------------------------------------------
|   AP4_FileWriter::Write
+---------------------------------------------------------------------*/
AP4_Result
AP4_FileWriter::Write(AP4_ByteStream& stream)
{
    // get the file type
    AP4_FtypAtom* file_type = m_File.GetFileType();

    // write the ftyp atom (always first)
    if (file_type) file_type->Write(stream);

    // write the top-level atoms
    AP4_List<AP4_Atom>::Item* atom_item = m_File.GetOtherAtoms().FirstItem();
    while (atom_item) {
        AP4_Atom* atom = atom_item->GetData();
        atom->Write(stream);
        atom_item = atom_item->GetNext();
    }
             
    // get the movie object
    AP4_Movie* movie = m_File.GetMovie();
    if (movie == NULL) return AP4_SUCCESS;
    
    // compute the final offset of the sample data in mdat
    AP4_UI64 data_offset = 0;
    if (file_type) data_offset += file_type->GetSize();
    m_File.GetOtherAtoms().Apply(AP4_AtomSizeAdder(data_offset));
    data_offset += movie->GetMoovAtom()->GetSize();
    data_offset += AP4_ATOM_HEADER_SIZE; // mdat header

    // adjust the tracks (assuming all chunk offsets were 0-based)
    AP4_List<AP4_Track>::Item* track_item = movie->GetTracks().FirstItem();
    while (track_item) {
        AP4_Track* track = track_item->GetData();
        track->GetTrakAtom()->AdjustChunkOffsets((int)data_offset);
        track_item = track_item->GetNext();
    }

    // write the moov atom
    movie->GetMoovAtom()->Write(stream);
    
    // create and write the media data (mdat)
    stream.WriteUI32(0);
    stream.Write("mdat", 4);
    AP4_Track* track = movie->GetTracks().FirstItem()->GetData();
    AP4_Cardinal sample_count = track->GetSampleCount();
    for (AP4_Ordinal i=0; i<sample_count; i++) {
        AP4_Sample sample;
        AP4_DataBuffer data;
        track->ReadSample(i, sample, data);
        stream.Write(data.GetData(), data.GetDataSize());
    }

    // TODO: update the mdat size

    return AP4_SUCCESS;
}
