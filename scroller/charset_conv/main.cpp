#include <fstream>
#include <map>
#include <vector>

using CharData = std::array<char,8>;
using CharSet = std::vector<CharData>;
using CharMapping = std::vector<std::pair<char,char>>;

class Exception : public std::exception
{
public:
    Exception(const char* msg) : m_msg(msg) {};
    virtual const char* what() const throw() override { return m_msg; }
private:
    const char* m_msg;
};

struct TileSet
{
    int get(int index, int y, int x) const
    {
        if (checkRange(index,y,x))
            return memory[offset(index,y,x)];
        return 0;
    }
    void set(int index, int y, int x, int data)
    {
        if (checkRange(index,y,x))
            memory[offset(index,y,x)] = data;
    }
    bool checkRange(int index, int y, int x) const { return (index >= 0) && (index < numTiles) && (y >= 0) && (y < tileHeight) && (x >= 0) && (x < tileWidth); }
    int offset(int index, int y, int x) const { return x+(((index*tileHeight)+y)*tileWidth);}

    int numTiles;
    int tileWidth;
    int tileHeight;
    std::vector<int> memory;
};

struct CharPadData
{
    CharSet charset;
    TileSet tileset;
};


struct SymbolData
{
    void setY(int y, int lineIndex)
    {
        while (static_cast<int>(lineIndexes.size()) <= y)
            lineIndexes.push_back(0);
        lineIndexes[y] = lineIndex;
    }
    bool empty() const
    {
        for (int v : lineIndexes)
            if (v)
                return false;
        return true;
    }

    int width = 0;
    std::vector<int> lineIndexes;
};

bool startswith(std::vector<char> const& v1, std::vector<char> const& v2)
{
    if (v1.size() < v2.size())
        return false;
    for (auto it1=v1.begin(), it2=v2.begin(), itEnd = v1.end(); it1 != itEnd; it1++, ++it2++)
        if (*it1 != *it2)
            return false;
    return true;
}



struct SymbolSet
{
    SymbolSet()
    {
        linedata.push_back(std::vector<char>{0});
    }

    void addLine(int tile, int y, std::vector<char>&& data)
    {
        int width = data.size();
        if (width == 0)
        {
            addSymbolData(tile, width, y, 0);
            return;
        }

        int index = 0;

        while (y >= height)
        {
            for (auto& symbol : symboldata)
                symbol.setY(height, 0);
            ++height;
        }

        //empty line always has max width
        growLine(0, width);

        for (std::vector<char>& existingLine : linedata)
        {
            if (startswith(existingLine, data))
            {
                addSymbolData(tile, width, y, index);
                return;
            }
            else if (startswith(data, existingLine))
            {
                existingLine = data;
                addSymbolData(tile, width, y, index);
                return;
            }
            ++index;
        }
        linedata.push_back(std::move(data));
        addSymbolData(tile, width, y, index);
    }

    void addSymbolData(int tile, int width, int y, int lineIndex)
    {
        if (tile >= static_cast<int>(symboldata.size()))
            symboldata.resize(tile+1);
        SymbolData& rData = symboldata[tile];
        rData.width = std::max(rData.width, width);
        rData.setY(y, lineIndex);
        for (int line : rData.lineIndexes)
            growLine(line, rData.width);
    }

    void growLine(int lineindex, int width)
    {
        auto& line = linedata[lineindex];
        while (static_cast<int>(line.size()) < width)
            line.push_back(0);
    }

    void discardEmptySymbolsAtEnd()
    {
        while (symboldata.back().empty())
            symboldata.pop_back();
    }

    void calcLineOffsets()
    {
        unsigned short combinedOffset = 0;
        for(auto const& line : linedata)
        {
            unsigned short nextOffset = combinedOffset + static_cast<unsigned short>(line.size());
            if ((combinedOffset & 0xF00) != (nextOffset & 0xF00))
            {
                unsigned short padding = (0x100 - (combinedOffset & 0xFF));
                std::vector<char> fill(padding, 0);
                pagedlinedata.insert(pagedlinedata.end(), fill.begin(), fill.end());
                combinedOffset += padding;
                nextOffset += padding;
            }
            pagedlinedata.insert(pagedlinedata.end(), line.begin(), line.end());
            pagedlineoffsets.push_back(combinedOffset);
            combinedOffset = nextOffset;
        }
    }

    int height = 0;
    std::vector<SymbolData> symboldata;
    std::vector<std::vector<char>> linedata;
    std::vector<char> pagedlinedata;
    std::vector<unsigned short> pagedlineoffsets;
};

struct OutputData
{
    CharSet charset[2];
    SymbolSet symbols;
    CharMapping charMapping;
};


int readInt8(std::ifstream& input)
{
    char data;
    input.read(&data, 1);
    return data;
}

int readInt16(std::ifstream& input)
{
    unsigned char data[2] = {0,0};
    input.read(reinterpret_cast<char*>(data), 2);
    int gcount = input.gcount();
    if (gcount != 2)
    {
        int flags = input.flags();
        int eof = std::ifstream::eofbit;
        int fail = std::ifstream::failbit;
        int bad = std::ifstream::badbit;
        int good = std::ifstream::goodbit;
        bool beof = input.eof();
        int bp = 0;
    }
    return data[0] + data[1]*256;
}

CharSet readCharSet(std::ifstream& input, int count)
{
    std::vector<CharData> result(count);
    input.read(reinterpret_cast<char*>(result.data()), 8 * count);
    return result;
}

TileSet readTileSet(std::ifstream& input, int count, int width, int height)
{
    TileSet result;
    result.numTiles = count;
    result.tileWidth = width;
    result.tileHeight = height;
    int len = width*height*count;
    for (int n=0; n<len; ++n)
    {
        int data = readInt16(input);
        result.memory.push_back(data);
    }
    return result;
}

bool readCTM4(std::ifstream& input, CharPadData& out)
{
    input.ignore(6);
    int numChars = readInt16(input)+1;
    int numTiles = readInt8(input)+1;
    int tileWidth = readInt8(input);
    int tileHeight = readInt8(input);
    input.ignore(4);
    int expanded = readInt8(input);
    if (expanded) // EXPANDED DATA
        return false;
    input.ignore(4);

    out.charset = readCharSet(input, numChars);
    input.ignore(numChars);
    out.tileset = readTileSet(input, numTiles, tileWidth, tileHeight);
    return true;
}

bool readCTM5(std::ifstream& input, CharPadData& out)
{
    input.ignore(5);
    int flags = readInt8(input);
    if (flags & 2) // EXPANDED DATA
        return false;
    int numChars = readInt16(input)+1;
    int numTiles = readInt16(input)+1;
    int tileWidth = readInt8(input);
    int tileHeight = readInt8(input);
    input.ignore(4);

    out.charset = readCharSet(input, numChars);
    input.ignore(numChars);
    out.tileset = readTileSet(input, numTiles, tileWidth, tileHeight);
    return true;
}

bool readFile(const char* filename, CharPadData& out)
{
    std::ifstream input(filename, std::ifstream::in | std::ifstream::binary);
    char header[4];
    input.read(header, 4);

    if ((header[0] == 'C') && (header[1] == 'T') && (header[2] == 'M'))
    {
        if (header[3] == 4)
            return readCTM4(input, out);
        else if (header[3] == 5)
            return readCTM5(input, out);
    }
    return false;
}

void convert(CharPadData const& in, OutputData& out, int spaceWidth)
{
    TileSet const& tileSet = in.tileset;

    //paare von tiles finden
    using CharPair = std::pair<char, char>;
    std::map<CharPair, int> charPairMap;
    int iLastCharIndex = 0;
    charPairMap.emplace(CharPair(0,0), 0);
	out.charMapping.emplace_back(CharPair(0,0));
    out.charset[0].emplace_back(in.charset[0]);
    out.charset[1].emplace_back(in.charset[0]);

    for (int t=0; t<tileSet.numTiles; ++t)
        for (int y=0; y<tileSet.tileHeight; ++y)
        {
            std::vector<char> lineData;
            for (int x=-1; x<tileSet.tileWidth; ++x)
            {
                CharPair pair(tileSet.get(t,y,x), tileSet.get(t,y,x+1));
                int iFoundIndex = 0;
                auto it = charPairMap.find(pair);
                if (it == charPairMap.end())
                {
                    if (iLastCharIndex == 255)
                        throw Exception("too many character pairs");

                    charPairMap[pair] = ++iLastCharIndex;
                    iFoundIndex = iLastCharIndex;
                    out.charMapping.push_back(pair);
                    out.charset[0].emplace_back(in.charset[pair.first]);
                    out.charset[1].emplace_back(in.charset[pair.second]);
                }
                else                
                {
                    iFoundIndex = it->second;
                }
                lineData.push_back(iFoundIndex);
            }
            //remove zeroes at end
            while(lineData.back() == 0)
                lineData.pop_back();
            out.symbols.addLine(t, y, std::move(lineData));
        }

    //set width of <space> symbol;
    out.symbols.symboldata[0].width = spaceWidth;
    //erase zero symbols;
    out.symbols.discardEmptySymbolsAtEnd();
    //calc line offsets
    out.symbols.calcLineOffsets();

    if (iLastCharIndex > 255)
        throw Exception("Too many tile pairs");
}


void writeSymbolSet(const char* filename, SymbolSet const& data)
{
    size_t const widthtable_size = data.symboldata.size();
    size_t const ptrtable_size = data.symboldata.size() * data.height * 2;

    std::vector<char> widthtable;
    for(auto const& symbol : data.symboldata)
        widthtable.push_back(static_cast<char>(symbol.width));

    std::vector<char> lohitable;
    for (int y = 0; y < data.height; ++y)
        for(auto const& symbol : data.symboldata)
            lohitable.push_back(static_cast<char>((data.pagedlineoffsets[symbol.lineIndexes[y]]) & 0xFF));
    for (int y = 0; y < data.height; ++y)
        for(auto const& symbol : data.symboldata)
            lohitable.push_back(static_cast<char>(((data.pagedlineoffsets[symbol.lineIndexes[y]]) >> 8) & 0xFF));

    std::ofstream output(filename, std::ofstream::out | std::ifstream::binary);
    output.write(widthtable.data(), widthtable.size());
    output.write(lohitable.data(), lohitable.size());
    output.write(data.pagedlinedata.data(), data.pagedlinedata.size());
}

void writeCharSet(const char* filename, CharSet const& data)
{
    std::ofstream output(filename, std::ofstream::out | std::ifstream::binary);
    output.write(reinterpret_cast<char const*>(data.data()), data.size() * 8);
}

void writePackedCharSets(const char* filename, CharMapping const& mapping, CharSet const& src)
{
    std::ofstream output(filename, std::ofstream::out | std::ifstream::binary);
    for(auto const& pair : mapping)
        output.put(pair.first);
    for(auto const& pair : mapping)
        output.put(pair.second);
    output.write(reinterpret_cast<char const*>(src.data()), src.size() * 8);
}


int main(int argc, char *argv[])
{
    CharPadData charPadData;
    readFile("e:\\c64dev\\nd_proj\\scroller\\font2.ctm", charPadData);

    OutputData outData;
    convert(charPadData, outData, 4);

    writeCharSet("e:\\c64dev\\nd_proj\\scroller\\conv_charset_a.bin", outData.charset[0]);
    writeCharSet("e:\\c64dev\\nd_proj\\scroller\\conv_charset_b.bin", outData.charset[1]);
    writePackedCharSets("e:\\c64dev\\nd_proj\\scroller\\conv_packed.bin", outData.charMapping, charPadData.charset);
    writeSymbolSet("e:\\c64dev\\nd_proj\\scroller\\conv_symbols_paged.bin", outData.symbols);
}

