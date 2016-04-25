#include "serialize.h"
#include "hash.h"

template<typename Stream>
void WriteCompactSize(Stream& os, uint64_t nSize)
{
    if (nSize < 253)
    {
        unsigned char chSize = (unsigned char)nSize;
        WRITEDATA(os, chSize);
    }
    else if (nSize <= std::numeric_limits<unsigned short>::max())
    {
        unsigned char chSize = 253;
        unsigned short xSize = (unsigned short)nSize;
        WRITEDATA(os, chSize);
        WRITEDATA(os, xSize);
    }
    else if (nSize <= std::numeric_limits<unsigned int>::max())
    {
        unsigned char chSize = 254;
        unsigned int xSize = (unsigned int)nSize;
        WRITEDATA(os, chSize);
        WRITEDATA(os, xSize);
    }
    else
    {
        unsigned char chSize = 255;
        uint64_t xSize = nSize;
        WRITEDATA(os, chSize);
        WRITEDATA(os, xSize);
    }
    return;
}


template<typename Stream>
uint64_t ReadCompactSize(Stream& is)
{
    unsigned char chSize;
    READDATA(is, chSize);
    uint64_t nSizeRet = 0;
    if (chSize < 253)
    {
        nSizeRet = chSize;
    }
    else if (chSize == 253)
    {
        unsigned short xSize;
        READDATA(is, xSize);
        nSizeRet = xSize;
    }
    else if (chSize == 254)
    {
        unsigned int xSize;
        READDATA(is, xSize);
        nSizeRet = xSize;
    }
    else
    {
        uint64_t xSize;
        READDATA(is, xSize);
        nSizeRet = xSize;
    }
    if (nSizeRet > (uint64_t)MAX_SIZE)
        throw std::ios_base::failure("ReadCompactSize() : size too large");
    return nSizeRet;
}



// Template instantiation

template void WriteCompactSize<CAutoFile>(CAutoFile&, uint64_t);
template void WriteCompactSize<CDataStream>(CDataStream&, uint64_t);
template void WriteCompactSize<CHashWriter>(CHashWriter&, uint64_t);

template uint64_t ReadCompactSize<CAutoFile>(CAutoFile&);
template uint64_t ReadCompactSize<CDataStream>(CDataStream&);

//
//CBufferedFile
//

void CBufferedFile::setstate(short bits, const char *psz)
{
    state |= bits;
    if (state & exceptmask)
        throw std::ios_base::failure(psz);
}

bool CBufferedFile::Fill()
{
    unsigned int pos = (unsigned int)(nSrcPos % vchBuf.size());
    unsigned int readNow = (unsigned int)(vchBuf.size() - pos);
    unsigned int nAvail = (unsigned int)(vchBuf.size() - (nSrcPos - nReadPos) - nRewind);
    if (nAvail < readNow)
        readNow = nAvail;
    if (readNow == 0)
        return false;
    size_t read = fread((void*)&vchBuf[pos], 1, readNow, src);
    if (read == 0) {
        setstate(std::ios_base::failbit, feof(src) ? "CBufferedFile::Fill : end of file" : "CBufferedFile::Fill : fread failed");
        return false;
    } else {
        nSrcPos += read;
        return true;
    }
}

CBufferedFile::CBufferedFile(FILE *fileIn, uint64_t nBufSize, uint64_t nRewindIn, int nTypeIn, int nVersionIn) :
    src(fileIn), nSrcPos(0), nReadPos(0), nReadLimit(std::numeric_limits<uint64_t>::max()), nRewind(nRewindIn), vchBuf(nBufSize, 0),
    state(0), exceptmask(std::ios_base::badbit | std::ios_base::failbit), nType(nTypeIn), nVersion(nVersionIn) { }

bool CBufferedFile::good() const
{
    return state == 0;
}

bool CBufferedFile::eof() const
{
    return nReadPos == nSrcPos && feof(src);
}

CBufferedFile& CBufferedFile::read(char *pch, size_t nSize)
{
    if (nSize + nReadPos > nReadLimit)
        throw std::ios_base::failure("Read attempted past buffer limit");
    if (nSize + nRewind > vchBuf.size())
        throw std::ios_base::failure("Read larger than buffer size");
    while (nSize > 0) {
        if (nReadPos == nSrcPos)
            Fill();
        unsigned int pos = (unsigned int)(nReadPos % vchBuf.size());
        size_t nNow = nSize;
        if (nNow + pos > vchBuf.size())
            nNow = vchBuf.size() - pos;
        if (nNow + nReadPos > nSrcPos)
            nNow = (size_t)(nSrcPos - nReadPos);
        memcpy(pch, &vchBuf[pos], nNow);
        nReadPos += nNow;
        pch += nNow;
        nSize -= nNow;
    }
    return (*this);
}

uint64_t CBufferedFile::GetPos()
{
    return nReadPos;
}

bool CBufferedFile::SetPos(uint64_t nPos)
{
    nReadPos = nPos;
    if (nReadPos + nRewind < nSrcPos) {
        nReadPos = nSrcPos - nRewind;
        return false;
    } else if (nReadPos > nSrcPos) {
        nReadPos = nSrcPos;
        return false;
    } else {
        return true;
    }
}

bool CBufferedFile::Seek(uint64_t nPos)
{
    long nLongPos = (long)nPos;
    if (nPos != (uint64_t)nLongPos)
        return false;
    if (fseek(src, nLongPos, SEEK_SET))
        return false;
    nLongPos = ftell(src);
    nSrcPos = nLongPos;
    nReadPos = nLongPos;
    state = 0;
    return true;
}

bool CBufferedFile::SetLimit(uint64_t nPos)
{
    if (nPos < nReadPos)
        return false;
    nReadLimit = nPos;
    return true;
}

void CBufferedFile::FindByte(char ch)
{
    for ( ; ; ) {
        if (nReadPos == nSrcPos)
            Fill();
        if (vchBuf[nReadPos % vchBuf.size()] == ch)
            break;
        nReadPos++;
    }
}