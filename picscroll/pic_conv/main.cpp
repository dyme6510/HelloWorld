#include <fstream>
#include <map>
#include <vector>

using CharData = std::array<char,8>;
using CharSet = std::vector<CharData>;
using CharMapping = std::vector<std::pair<char,char>>;


struct PicCell{
    void normalize(){
        unicolor = ((color & 0xF0) >> 4 == (color & 0x0F));
        if (unicolor)
        {
            for (int i=0; i<8; ++i)
                bits[i] = 0;
        }
        else if (bits[0] & 0x80)
        {
            for (int i=0; i<8; ++i)
                bits[i] = ~bits[i];
            color = ((color & 0x0F) << 4) | ((color & 0xF0) >> 4);
        }
        bool bAllZero = true;
        for (int i=0; i<8; ++i)
            bAllZero &= bits[i] == 0;
        if (bAllZero)
            color = ((color & 0x0F) << 4) | (color & 0x0F);
        unicolor = ((color & 0xF0) >> 4 == (color & 0x0F));
        if (unicolor)
            for (int i=0; i<8; ++i)
                bits[i] = 0;
    }
    
    void initFlags(PicCell const& rCmp, bool bWildUnicolor){
        newColor = (color != rCmp.color);
        if (unicolor && bWildUnicolor)
        {
            for (int i=0; i<8; ++i)
                bits[i] = rCmp.bits[i];
            newBits = false;
        }
        else
        {        
            newBits = (bits[0]!=rCmp.bits[0]
                    || bits[1]!=rCmp.bits[1]
                    || bits[2]!=rCmp.bits[2]
                    || bits[3]!=rCmp.bits[3]
                    || bits[4]!=rCmp.bits[4]
                    || bits[5]!=rCmp.bits[5]
                    || bits[6]!=rCmp.bits[6]
                    || bits[7]!=rCmp.bits[7]);
        }
    }
    char color;
    char bits[8];
    bool newColor = true;
    bool newBits = true;
    bool unicolor = false;
};
using TPicData = std::vector<PicCell>;


TPicData readFiles(const char* mapfile, const char* scrfile)
{
    TPicData result;
    std::ifstream inputMap(mapfile, std::ifstream::in | std::ifstream::binary);
    std::ifstream inputSrc(scrfile, std::ifstream::in | std::ifstream::binary);

    PicCell currentCell;
    while (!inputSrc.eof())
    {
        inputSrc.read(&currentCell.color, 1);
        if (inputSrc.eof())
            break;
        inputMap.read(currentCell.bits, 8);
        currentCell.normalize();
        result.push_back(currentCell);
    }

    PicCell initialCell;
    initialCell.color = 0;
    for (int i=0; i<8; ++i)
        initialCell.bits[i] = 0;

    int const count = static_cast<int>(result.size());
    for (int i = count-1; i >= 0; --i)
    {
        bool bWildUnicolor = (i >= 40); // first row shall really be zero
        int const src = i+80;
        PicCell const& compare = (src >= count) ? initialCell : result[src];
        result[i].initFlags(compare, bWildUnicolor);
    }
    return result;
}

void writeData(TPicData const& data, const char* filenameStream, const char* filenameMask)
{
    std::ofstream outputMask(filenameMask, std::ofstream::out | std::ifstream::binary);
    std::ofstream outputStream(filenameStream, std::ofstream::out | std::ifstream::binary);
    size_t const count = data.size();

    size_t lines = count / 40;
    for (int line = lines-1; line >= 0; --line)
    {
        for (size_t column = 0; column < 20; ++column)
        {
            size_t index1 = line * 40 + column;
            PicCell const& rCell1 = data[index1];
            PicCell const& rCell2 = data[index1+20];
            char const mask = (rCell2.newColor ? 0x20 : 0)
                            | (rCell1.newColor ? 0x10 : 0)
                            | (rCell2.newBits ? 0x80 : 0)
                            | (rCell1.newBits ? 0x40 : 0);
            outputMask.put(mask);
            if (rCell1.newBits)
                outputStream.write(rCell1.bits, 8);
            if (rCell2.newBits)
                outputStream.write(rCell2.bits, 8);
            if (mask)
            {
                outputStream.put(rCell1.color);
                outputStream.put(rCell2.color);
            }
        }
    }
}



int main(int argc, char *argv[])
{
    TPicData rawData = readFiles("e:\\c64dev\\nd_proj\\picscroll\\held3.map", "e:\\c64dev\\nd_proj\\picscroll\\held3.scr");
    writeData(rawData, "e:\\c64dev\\nd_proj\\picscroll\\held3.stream", "e:\\c64dev\\nd_proj\\picscroll\\held3.mask");
}

